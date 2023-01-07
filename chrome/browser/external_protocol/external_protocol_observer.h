// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// ExternalProtocolObserver is responsible for handling messages from
// WebContents relating to external protocols.
class ExternalProtocolObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExternalProtocolObserver> {
 public:
  ExternalProtocolObserver(const ExternalProtocolObserver&) = delete;
  ExternalProtocolObserver& operator=(const ExternalProtocolObserver&) = delete;

  ~ExternalProtocolObserver() override;

  // content::WebContentsObserver overrides.
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

 private:
  explicit ExternalProtocolObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ExternalProtocolObserver>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_OBSERVER_H_
