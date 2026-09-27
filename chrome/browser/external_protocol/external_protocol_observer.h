// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

// ExternalProtocolObserver is responsible for handling messages from
// WebContents relating to external protocols.
class ExternalProtocolObserver : public content::WebContentsObserver {
 public:
  explicit ExternalProtocolObserver(content::WebContents* web_contents);

  ExternalProtocolObserver(const ExternalProtocolObserver&) = delete;
  ExternalProtocolObserver& operator=(const ExternalProtocolObserver&) = delete;

  ~ExternalProtocolObserver() override;

  // content::WebContentsObserver overrides.
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_
