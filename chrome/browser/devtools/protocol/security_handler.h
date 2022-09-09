// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_

#include "chrome/browser/devtools/protocol/security.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

class SecurityHandler : public protocol::Security::Backend,
                        public content::WebContentsObserver {
 public:
  SecurityHandler(content::WebContents* web_contents,
                  protocol::UberDispatcher* dispatcher);

  SecurityHandler(const SecurityHandler&) = delete;
  SecurityHandler& operator=(const SecurityHandler&) = delete;

  ~SecurityHandler() override;

  // Security::Backend:
  protocol::Response Enable() override;
  protocol::Response Disable() override;

 private:
  // WebContentsObserver overrides
  void DidChangeVisibleSecurityState() override;

  bool enabled_ = false;
  std::unique_ptr<protocol::Security::Frontend> frontend_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
