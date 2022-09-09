// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_observer.h"

#include "chrome/browser/external_protocol/external_protocol_handler.h"

using content::WebContents;

ExternalProtocolObserver::ExternalProtocolObserver(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ExternalProtocolObserver>(*web_contents) {}

ExternalProtocolObserver::~ExternalProtocolObserver() {
}

void ExternalProtocolObserver::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  // Ignore scroll events for allowing external protocol launch.
  if (event.GetType() != blink::WebInputEvent::Type::kGestureScrollBegin)
    ExternalProtocolHandler::PermitLaunchUrl();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExternalProtocolObserver);
