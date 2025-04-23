// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_HOST_H_
#define CHROME_BROWSER_GLIC_HOST_HOST_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"

class Profile;
namespace content {
class WebContents;
class RenderProcessHost;
}  // namespace content
namespace glic {
class GlicKeyedService;
class GlicPageHandler;
class WebUIContentsContainer;

// The host owns the WebUI that contains the main glic UI and the web client.
// TODO(crbug.com/409332639): Better encapsulate details here.
class Host {
 public:
  explicit Host(Profile* profile);
  ~Host();

  // Delete the owned web contents.
  void Shutdown();

  // Creates the web contents that will own the Glic WebUI.
  void CreateContents();

  // Called when a `GlicPageHandler` is created.
  void WebUIPageHandlerAdded(GlicPageHandler* page_handler);

  // Called when a `GlicPageHandler` is about to be destroyed.
  void WebUIPageHandlerRemoved(GlicPageHandler* page_handler);

  // Called when a glic guest (webview web contents) is added.
  void GuestAdded(content::WebContents* guest_contents);

  // Signals the glic WebUI that the glic window will be shown soon.
  void NotifyWindowIntentToShow();

  // Returns the page handler that owns the WebUI web contents.
  GlicPageHandler* FindPageHandlerForWebUiContents(
      const content::WebContents* webui_contents);

  // Called when a page handler's web client is created or destroyed.
  void SetWebClient(GlicPageHandler* page_handler,
                    GlicWebClientAccess* web_client);

  WebUIContentsContainer* contents_container() { return contents_.get(); }
  // Returns the WebUI web contents. May be null.
  content::WebContents* webui_contents();

  // Returns whether `contents` is the glic WebUI web contents.
  bool IsGlicWebUi(content::WebContents* contents);

  // Returns whether `host` is the glic WebUI render process host.
  bool IsGlicWebUiHost(content::RenderProcessHost* host);

  // Returns the list of page handlers for glic WebUI pages.
  std::vector<GlicPageHandler*> GetPageHandlersForTesting();

 private:
  GlicKeyedService& glic_service();

  struct PageHandlerInfo {
    raw_ptr<GlicPageHandler> page_handler = nullptr;
    raw_ptr<GlicWebClientAccess> web_client = nullptr;
  };
  PageHandlerInfo* FindInfo(GlicPageHandler* handler);
  PageHandlerInfo* FindInfoForWebUiContents(content::WebContents* web_contents);

  raw_ptr<Profile> profile_;
  // The set of live `GlicPageHandler`s.
  std::vector<PageHandlerInfo> page_handlers_;
  // Keep profile alive as long as the glic web contents. This object should be
  // destroyed when the profile needs to be destroyed.
  std::unique_ptr<WebUIContentsContainer> contents_;
  // The primary page handler. The first page handler which connects to a client
  // (normally, the feature's main client) is saved as the primary page handler.
  // The GlicWindowController supports only a single client, so this owns the
  // client it will support.
  raw_ptr<GlicPageHandler> primary_page_handler_ = nullptr;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_HOST_HOST_H_
