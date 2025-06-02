// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_QWAC_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_NET_QWAC_WEB_CONTENTS_OBSERVER_H_

#include "base/callback_list.h"
#include "base/supports_user_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

// QwacWebContentsObserver is responsible for observing 2-QWAC link headers
// on main frame navigations. It then creates the QwacStatus subclass which
// fetches the binding, verifies it, and stores the resulting 2-QWAC state on
// content::Page's user data.
//
// TODO(crbug.com/392931069): Hook up actual verification of the 2-QWAC.
// TODO(crbug.com/392931069): Add histograms.
class QwacWebContentsObserver : public content::WebContentsObserver {
 public:
  class QwacStatus : public content::PageUserData<QwacStatus> {
   public:
    using CallbackList = base::OnceCallbackList<void()>;

    ~QwacStatus() override;
    QwacStatus(const QwacStatus&) = delete;
    QwacStatus& operator=(const QwacStatus&) = delete;

    bool is_finished() const { return is_finished_; }
    net::X509Certificate* tls_cert() const { return tls_cert_.get(); }

    const std::optional<std::string>& GetResponseBodyForTesting() const {
      return response_body_;
    }

    // Registers a callback to be run when processing the 2-QWAC has completed.
    // Should only be called if `is_finished()` is false. If the QwacStatus is
    // destroyed before the processing finishes, the callback will not be run.
    base::CallbackListSubscription RegisterCallback(
        CallbackList::CallbackType cb);

   private:
    QwacStatus(
        content::Page& page,
        scoped_refptr<net::X509Certificate> tls_cert,
        GURL qwac_url,
        const url::Origin& initiator,
        mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

    friend content::PageUserData<QwacStatus>;
    PAGE_USER_DATA_KEY_DECL();

    void On2QwacDownloadComplete(std::optional<std::string> response_body);

    std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
    scoped_refptr<net::X509Certificate> tls_cert_;

    // TODO(crbug.com/392931069): remove this once the feature can be tested
    // end-to-end.
    std::optional<std::string> response_body_;

    bool is_finished_ = false;
    CallbackList callback_list_;
  };

  // Observes a Tab and will update which WebContents is being observed
  // automatically when the WebContents in the tab changes.
  explicit QwacWebContentsObserver(tabs::TabInterface& tab);

  // Observes a single WebContents.
  explicit QwacWebContentsObserver(content::WebContents* web_contents);

  ~QwacWebContentsObserver() override;

  QwacWebContentsObserver(const QwacWebContentsObserver&) = delete;
  QwacWebContentsObserver& operator=(const QwacWebContentsObserver&) = delete;

  // TabInterface::WillDiscardContentsCallback:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  base::CallbackListSubscription tab_subscription_;
};

#endif  // CHROME_BROWSER_NET_QWAC_WEB_CONTENTS_OBSERVER_H_
