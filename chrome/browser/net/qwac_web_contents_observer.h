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
class QwacWebContentsObserver : public content::WebContentsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(NetCertVerifier2QwacLinkProcessingResult)
  enum class QwacLinkProcessingResult {
    kQwacStatusAlreadyPresent = 0,
    kUnacceptableSslInfo = 1,
    kNoQwacLinkHeader = 2,
    kInvalidQwacLinkHeader = 3,
    kNonrelativeQwacLinkUrl = 4,
    kDownloadFailed = 5,
    k2QwacVerificationFailed = 6,
    kValid2Qwac = 7,
    kDestroyedBeforeFinish = 8,

    kMaxValue = kDestroyedBeforeFinish,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NetCertVerifier2QwacLinkProcessingResult)

  class QwacStatus : public content::PageUserData<QwacStatus> {
   public:
    using CallbackList = base::OnceCallbackList<void()>;

    ~QwacStatus() override;
    QwacStatus(const QwacStatus&) = delete;
    QwacStatus& operator=(const QwacStatus&) = delete;

    // Returns true if 2-QWAC processing finished.  `verified_2qwac_cert()`
    // should be checked to see if verification succeeded.
    bool is_finished() const { return is_finished_; }

    // Returns the verified 2-QWAC certificate chain, if 2-QWAC verification
    // succeeded, or nullptr if verification failed. Should only be called if
    // `is_finished()` is true.
    net::X509Certificate* verified_2qwac_cert() const {
      CHECK(is_finished());
      return verified_2qwac_.get();
    }

    net::X509Certificate* tls_cert() const { return tls_cert_.get(); }

    // Registers a callback to be run when processing the 2-QWAC has completed.
    // Should only be called if `is_finished()` is false. If the QwacStatus is
    // destroyed before the processing finishes, the callback will not be run.
    base::CallbackListSubscription RegisterCallback(
        CallbackList::CallbackType cb);

   private:
    QwacStatus(
        content::Page& page,
        std::string hostname,
        scoped_refptr<net::X509Certificate> tls_cert,
        GURL qwac_url,
        const url::Origin& initiator,
        mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

    friend content::PageUserData<QwacStatus>;
    PAGE_USER_DATA_KEY_DECL();

    void On2QwacDownloadComplete(std::optional<std::string> response_body);
    void On2QwacVerificationComplete(
        const scoped_refptr<net::X509Certificate>& verified_2qwac);

    std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
    std::string hostname_;
    scoped_refptr<net::X509Certificate> tls_cert_;

    // True when fetching and verifying the binding is complete. If this is
    // true, `verified_2qwac_` can be checked to see if verification
    // succeeded.
    bool is_finished_ = false;

    // The 2-QWAC certificate chain from verifying the binding. This will be
    // null if verifying the 2-qwac binding failed, or if `is_finished_` is
    // false.
    scoped_refptr<net::X509Certificate> verified_2qwac_;

    CallbackList callback_list_;

    base::WeakPtrFactory<QwacStatus> weak_ptr_factory_{this};
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
