// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_web_view.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {

TestAssistantWebView::TestAssistantWebView() = default;

TestAssistantWebView::~TestAssistantWebView() = default;

void TestAssistantWebView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TestAssistantWebView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

gfx::NativeView TestAssistantWebView::GetNativeView() {
  // Not yet implemented for unittests.
  return nullptr;
}

bool TestAssistantWebView::GoBack() {
  // Not yet implemented for unittests.
  return false;
}

void TestAssistantWebView::Navigate(const GURL& url) {
  // Simulate navigation by notifying |observers_| of the expected event that
  // would normally signal navigation completion. We do this asynchronously to
  // more accurately simulate real-world conditions.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](const base::WeakPtr<TestAssistantWebView>& self) {
                       if (self) {
                         for (auto& observer : self->observers_)
                           observer.DidStopLoading();
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

}  // namespace ash
