// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/lifecycle/aw_contents_lifecycle_notifier.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/not_fatal_until.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentsLifecycleNotifier_jni.h"

using base::android::AttachCurrentThread;
using content::BrowserThread;

namespace android_webview {

namespace {

AwContentsLifecycleNotifier::AwContentsState CalculateState(
    bool is_attached_to_window,
    bool is_window_visible) {
  // Can't assume the sequence of Attached, Detached, Visible, Invisible event
  // because the app could changed it; Calculate the state here.
  if (is_attached_to_window) {
    return is_window_visible
               ? AwContentsLifecycleNotifier::AwContentsState::kForeground
               : AwContentsLifecycleNotifier::AwContentsState::kBackground;
  }
  return AwContentsLifecycleNotifier::AwContentsState::kDetached;
}

AwContentsLifecycleNotifier* g_instance = nullptr;

}  // namespace

AwContentsLifecycleNotifier::AwContentsData::AwContentsData() = default;

AwContentsLifecycleNotifier::AwContentsData::AwContentsData(
    AwContentsData&& data) = default;

AwContentsLifecycleNotifier::AwContentsData::~AwContentsData() = default;

// static
AwContentsLifecycleNotifier& AwContentsLifecycleNotifier::GetInstance() {
  DCHECK(g_instance);
  g_instance->EnsureOnValidSequence();
  return *g_instance;
}

AwContentsLifecycleNotifier::AwContentsLifecycleNotifier(
    OnLoseForegroundCallback on_lose_foreground_callback)
    : on_lose_foreground_callback_(std::move(on_lose_foreground_callback)) {
  EnsureOnValidSequence();
  DCHECK(!g_instance);
  g_instance = this;
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(Java_AwContentsLifecycleNotifier_getInstance(env));
}

AwContentsLifecycleNotifier::~AwContentsLifecycleNotifier() {
  EnsureOnValidSequence();
  DCHECK(g_instance);
  g_instance = nullptr;
}

void AwContentsLifecycleNotifier::OnWebViewCreated(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  has_aw_contents_ever_created_ = true;
  bool first_created = !HasAwContentsInstance();
  DCHECK(!base::Contains(aw_contents_to_data_, aw_contents));

  aw_contents_to_data_.emplace(aw_contents, AwContentsData());
  state_count_[ToIndex(AwContentsState::kDetached)]++;
  UpdateAppState();

  if (first_created) {
    Java_AwContentsLifecycleNotifier_onFirstWebViewCreated(
        AttachCurrentThread(), java_ref_);
  }
}

void AwContentsLifecycleNotifier::OnWebViewDestroyed(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const auto it = aw_contents_to_data_.find(aw_contents);
  CHECK(it != aw_contents_to_data_.end(), base::NotFatalUntil::M130);

  state_count_[ToIndex(it->second.aw_content_state)]--;
  DCHECK(state_count_[ToIndex(it->second.aw_content_state)] >= 0);
  aw_contents_to_data_.erase(it);
  UpdateAppState();

  if (!HasAwContentsInstance()) {
    Java_AwContentsLifecycleNotifier_onLastWebViewDestroyed(
        AttachCurrentThread(), java_ref_);
  }
}

void AwContentsLifecycleNotifier::OnWebViewAttachedToWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->attached_to_window = true;
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewDetachedFromWindow(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->attached_to_window = false;
  DCHECK(data->aw_content_state != AwContentsState::kDetached);
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeVisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->window_visible = true;
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::OnWebViewWindowBeInvisible(
    const AwContents* aw_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* data = GetAwContentsData(aw_contents);
  data->window_visible = false;
  OnAwContentsStateChanged(data);
}

void AwContentsLifecycleNotifier::AddObserver(
    WebViewAppStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
  observer->OnAppStateChanged(app_state_);
}

void AwContentsLifecycleNotifier::RemoveObserver(
    WebViewAppStateObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

std::vector<const AwContents*> AwContentsLifecycleNotifier::GetAllAwContents()
    const {
  std::vector<const AwContents*> result;
  result.reserve(aw_contents_to_data_.size());
  for (auto& it : aw_contents_to_data_)
    result.push_back(it.first);
  return result;
}

size_t AwContentsLifecycleNotifier::ToIndex(AwContentsState state) const {
  size_t index = static_cast<size_t>(state);
  DCHECK(index < std::size(state_count_));
  return index;
}

void AwContentsLifecycleNotifier::OnAwContentsStateChanged(
    AwContentsLifecycleNotifier::AwContentsData* data) {
  AwContentsLifecycleNotifier::AwContentsState state =
      CalculateState(data->attached_to_window, data->window_visible);
  if (data->aw_content_state == state)
    return;
  state_count_[ToIndex(data->aw_content_state)]--;
  DCHECK(state_count_[ToIndex(data->aw_content_state)] >= 0);
  state_count_[ToIndex(state)]++;
  data->aw_content_state = state;
  UpdateAppState();
}

void AwContentsLifecycleNotifier::UpdateAppState() {
  WebViewAppStateObserver::State state;
  if (state_count_[ToIndex(AwContentsState::kForeground)] > 0)
    state = WebViewAppStateObserver::State::kForeground;
  else if (state_count_[ToIndex(AwContentsState::kBackground)] > 0)
    state = WebViewAppStateObserver::State::kBackground;
  else if (state_count_[ToIndex(AwContentsState::kDetached)] > 0)
    state = WebViewAppStateObserver::State::kUnknown;
  else
    state = WebViewAppStateObserver::State::kDestroyed;
  if (state != app_state_) {
    bool previous_in_foreground =
        app_state_ == WebViewAppStateObserver::State::kForeground;

    app_state_ = state;
    for (auto& observer : observers_) {
      observer.OnAppStateChanged(app_state_);
    }
    if (previous_in_foreground && on_lose_foreground_callback_) {
      on_lose_foreground_callback_.Run();
    }

    Java_AwContentsLifecycleNotifier_onAppStateChanged(
        AttachCurrentThread(), java_ref_, static_cast<jint>(app_state_));
  }
}

bool AwContentsLifecycleNotifier::HasAwContentsInstance() const {
  for (size_t i = 0; i < std::size(state_count_); i++) {
    if (state_count_[i] > 0)
      return true;
  }
  return false;
}

AwContentsLifecycleNotifier::AwContentsData*
AwContentsLifecycleNotifier::GetAwContentsData(const AwContents* aw_contents) {
  DCHECK(base::Contains(aw_contents_to_data_, aw_contents));
  return &aw_contents_to_data_.at(aw_contents);
}

void AwContentsLifecycleNotifier::InitForTesting() {  // IN-TEST
  Java_AwContentsLifecycleNotifier_initialize(        // IN-TEST
      AttachCurrentThread());
}

}  // namespace android_webview
