// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_
#define CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_

#include <map>
#include <memory>
#include <set>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom.h"

namespace content {
class WebContents;
}

class InsecureSensitiveInputDriver;

// This object updates the NavigationEntry's SSLStatus UserData object when
// sensitive input events occur that may impact Chrome's determination of the
// page's security level.
//
// This class holds a map of InsecureSensitiveInputDrivers, which accept
// mojom::InsecureInputService notifications from renderers. There is at most
// one factory per WebContents, and one driver per render frame.
class InsecureSensitiveInputDriverFactory
    : public content::WebContentsObserver,
      public content::WebContentsUserData<InsecureSensitiveInputDriverFactory> {
 public:
  ~InsecureSensitiveInputDriverFactory() override;

  static InsecureSensitiveInputDriverFactory* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Finds or creates a factory for the |web_contents| and creates an
  // |InsecureSensitiveInputDriver| for the target |render_frame_host|.
  static void BindDriver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::InsecureInputService> receiver);

  // Creates a |InsecureSensitiveInputDriver| for the specified
  // |render_frame_host| and adds it to the |frame_driver_map_|.
  InsecureSensitiveInputDriver* GetOrCreateDriverForFrame(
      content::RenderFrameHost* render_frame_host);

  // This method is called when there is a message notifying the browser
  // process that the user edited a field in a non-secure context.
  void DidEditFieldInInsecureContext();

  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

 private:
  explicit InsecureSensitiveInputDriverFactory(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      InsecureSensitiveInputDriverFactory>;

  std::map<content::RenderFrameHost*,
           std::unique_ptr<InsecureSensitiveInputDriver>>
      frame_driver_map_;

  std::set<content::RenderFrameHost*> frames_with_visible_password_fields_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(InsecureSensitiveInputDriverFactory);
};

#endif  // CHROME_BROWSER_SSL_INSECURE_SENSITIVE_INPUT_DRIVER_FACTORY_H_
