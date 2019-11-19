// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"

#include <utility>

#include "base/stl_util.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {
// Creates or retrieves the |user_data| object in the SSLStatus attached to the
// WebContents' NavigationEntry.
security_state::SSLStatusInputEventData* GetOrCreateSSLStatusInputEventData(
    content::WebContents* web_contents) {
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  // We aren't guaranteed to always have a navigation entry.
  if (!entry)
    return nullptr;

  content::SSLStatus& ssl = entry->GetSSL();
  security_state::SSLStatusInputEventData* input_events =
      static_cast<security_state::SSLStatusInputEventData*>(
          ssl.user_data.get());
  if (!input_events) {
    ssl.user_data = std::make_unique<security_state::SSLStatusInputEventData>();
    input_events = static_cast<security_state::SSLStatusInputEventData*>(
        ssl.user_data.get());
  }
  return input_events;
}
}  // namespace

InsecureSensitiveInputDriverFactory::~InsecureSensitiveInputDriverFactory() {}

// static
InsecureSensitiveInputDriverFactory*
InsecureSensitiveInputDriverFactory::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  InsecureSensitiveInputDriverFactory* factory = FromWebContents(web_contents);

  if (!factory) {
    content::WebContentsUserData<InsecureSensitiveInputDriverFactory>::
        CreateForWebContents(web_contents);
    factory = FromWebContents(web_contents);
    DCHECK(factory);
  }
  return factory;
}

// static
void InsecureSensitiveInputDriverFactory::BindDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::InsecureInputService> receiver) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  InsecureSensitiveInputDriverFactory* factory =
      GetOrCreateForWebContents(web_contents);

  InsecureSensitiveInputDriver* driver =
      factory->GetOrCreateDriverForFrame(render_frame_host);

  driver->BindInsecureInputServiceReceiver(std::move(receiver));
}

InsecureSensitiveInputDriver*
InsecureSensitiveInputDriverFactory::GetOrCreateDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto insertion_result =
      frame_driver_map_.insert(std::make_pair(render_frame_host, nullptr));
  if (insertion_result.second) {
    insertion_result.first->second =
        std::make_unique<InsecureSensitiveInputDriver>(render_frame_host);
  }
  return insertion_result.first->second.get();
}

void InsecureSensitiveInputDriverFactory::DidEditFieldInInsecureContext() {
  security_state::SSLStatusInputEventData* input_events =
      GetOrCreateSSLStatusInputEventData(web_contents());
  if (!input_events)
    return;

  // If the first field edit in the web contents was just performed,
  // update the SSLStatusInputEventData.
  if (!input_events->input_events()->insecure_field_edited) {
    input_events->input_events()->insecure_field_edited = true;
    web_contents()->DidChangeVisibleSecurityState();
  }
}

void InsecureSensitiveInputDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

InsecureSensitiveInputDriverFactory::InsecureSensitiveInputDriverFactory(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InsecureSensitiveInputDriverFactory)
