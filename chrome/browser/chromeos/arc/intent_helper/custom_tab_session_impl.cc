// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/custom_tab_session_impl.h"

#include <utility>

#include "ash/public/cpp/arc_custom_tab.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/aura/window.h"

// static
arc::mojom::CustomTabSessionPtr CustomTabSessionImpl::Create(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab) {
  if (!custom_tab)
    return nullptr;

  // This object will be deleted when the mojo connection is closed.
  auto* tab =
      new CustomTabSessionImpl(std::move(web_contents), std::move(custom_tab));
  arc::mojom::CustomTabSessionPtr ptr;
  tab->Bind(&ptr);
  return ptr;
}

CustomTabSessionImpl::CustomTabSessionImpl(
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ash::ArcCustomTab> custom_tab)
    : ArcCustomTabModalDialogHost(std::move(custom_tab),
                                  std::move(web_contents)),
      binding_(this),
      weak_ptr_factory_(this) {
  aura::Window* window = web_contents_->GetNativeView();
  custom_tab_->Attach(window);
  window->Show();
}

CustomTabSessionImpl::~CustomTabSessionImpl() {
  // Keep in sync with ArcCustomTabsSessionEndReason in
  // tools/metrics/histograms/enums.xml.
  enum class SessionEndReason {
    CLOSED = 0,
    FORWARDED_TO_NORMAL_TAB = 1,
    kMaxValue = FORWARDED_TO_NORMAL_TAB,
  } session_end_reason = forwarded_to_normal_tab_
                             ? SessionEndReason::FORWARDED_TO_NORMAL_TAB
                             : SessionEndReason::CLOSED;
  UMA_HISTOGRAM_ENUMERATION("Arc.CustomTabs.SessionEndReason",
                            session_end_reason);
  auto elapsed = lifetime_timer_.Elapsed();
  UMA_HISTOGRAM_LONG_TIMES("Arc.CustomTabs.SessionLifetime2.All", elapsed);
  switch (session_end_reason) {
    case SessionEndReason::CLOSED:
      UMA_HISTOGRAM_LONG_TIMES("Arc.CustomTabs.SessionLifetime2.Closed",
                               elapsed);
      break;
    case SessionEndReason::FORWARDED_TO_NORMAL_TAB:
      UMA_HISTOGRAM_LONG_TIMES(
          "Arc.CustomTabs.SessionLifetime2.ForwardedToNormalTab", elapsed);
      break;
  }
}

void CustomTabSessionImpl::OnOpenInChromeClicked() {
  forwarded_to_normal_tab_ = true;
}

void CustomTabSessionImpl::Bind(arc::mojom::CustomTabSessionPtr* ptr) {
  binding_.Bind(mojo::MakeRequest(ptr));
  binding_.set_connection_error_handler(base::BindOnce(
      &CustomTabSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));
}

// Deletes this object when the mojo connection is closed.
void CustomTabSessionImpl::Close() {
  delete this;
}
