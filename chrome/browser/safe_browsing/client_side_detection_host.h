// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/db/database_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/gurl.h"

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionService;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
// TODO(noelutz): move all client-side detection IPCs to this class.
class ClientSideDetectionHost : public content::WebContentsObserver,
                                public SafeBrowsingUIManager::Observer {
 public:
  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid until WebContentsDestroyed is called.
  static std::unique_ptr<ClientSideDetectionHost> Create(
      content::WebContents* tab);
  ~ClientSideDetectionHost() override;

  // From content::WebContentsObserver.  If we navigate away we cancel all
  // pending callbacks that could show an interstitial, and check to see whether
  // we should classify the new URL.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const content::mojom::ResourceLoadInfo& resource_load_info) override;

  // Called when the SafeBrowsingService found a hit with one of the
  // SafeBrowsing lists.  This method is called on the UI thread.
  void OnSafeBrowsingHit(
      const security_interstitials::UnsafeResource& resource) override;

  virtual scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

  BrowseInfo* GetBrowseInfo() const { return browse_info_.get(); }

 protected:
  explicit ClientSideDetectionHost(content::WebContents* tab);

  // From content::WebContentsObserver.
  void WebContentsDestroyed() override;

  // Used for testing.
  void set_safe_browsing_managers(
      SafeBrowsingUIManager* ui_manager,
      SafeBrowsingDatabaseManager* database_manager);

  // Called when pre-classification checks are done for the malware classifiers.
  // Overridden in test.
  virtual void OnMalwarePreClassificationDone(bool should_classify);

 private:
  friend class ClientSideDetectionHostTestBase;
  class ShouldClassifyUrlRequest;
  friend class ShouldClassifyUrlRequest;

  // Called when pre-classification checks are done for the phishing
  // classifiers.
  void OnPhishingPreClassificationDone(bool should_classify);

  // |verdict| is an encoded ClientPhishingRequest protocol message, |result| is
  // the outcome of the renderer classification.
  void PhishingDetectionDone(mojom::PhishingDetectorResult result,
                             const std::string& verdict);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if |is_phishing| is true.
  // Otherwise, we do nothing.  Called in UI thread.
  void MaybeShowPhishingWarning(GURL phishing_url, bool is_phishing);

  // Callback that is called when the malware IP server ping back is
  // done. Display an interstitial if |is_malware| is true.
  // Otherwise, we do nothing.  Called in UI thread.
  void MaybeShowMalwareWarning(GURL original_url, GURL malware_url,
                               bool is_malware);

  // Callback that is called when the browser feature extractor is done.
  // This method is responsible for deleting the request object.  Called on
  // the UI thread.
  void FeatureExtractionDone(bool success,
                             std::unique_ptr<ClientPhishingRequest> request);

  // Start malware classification once the onload handler was called and
  // malware pre-classification checks are done and passed.
  void MaybeStartMalwareFeatureExtraction();

  // Function to be called when the browser malware feature extractor is done.
  // Called on the UI thread.
  void MalwareFeatureExtractionDone(
      bool success,
      std::unique_ptr<ClientMalwareRequest> request);

  // Update the entries in browse_info_->ips map.
  void UpdateIPUrlMap(const std::string& ip,
                      const std::string& url,
                      const std::string& method,
                      const std::string& referrer,
                      const content::ResourceType resource_type);

  // Inherited from WebContentsObserver.  This is called once the page is
  // done loading.
  void DidStopLoading() override;

  // Returns true if the user has seen a regular SafeBrowsing
  // interstitial for the current page.  This is only true if the user has
  // actually clicked through the warning.  This method is called on the UI
  // thread.
  bool DidShowSBInterstitial() const;

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service(ClientSideDetectionService* service);

  // This pointer may be NULL if client-side phishing detection is disabled.
  ClientSideDetectionService* csd_service_;
  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  // Keep a handle to the latest classification request so that we can cancel
  // it if necessary.
  scoped_refptr<ShouldClassifyUrlRequest> classification_request_;
  // Browser-side feature extractor.
  std::unique_ptr<BrowserFeatureExtractor> feature_extractor_;
  // Keeps some info about the current page visit while the renderer
  // classification is going on.  Since we cancel classification on
  // every page load we can simply keep this data around as a member
  // variable.  This information will be passed on to the feature extractor.
  std::unique_ptr<BrowseInfo> browse_info_;
  // Redirect chain that leads to the first page of the current host. We keep
  // track of this for browse_info_.
  std::vector<GURL> cur_host_redirects_;
  // Current host, used to help determine cur_host_redirects_.
  std::string cur_host_;
  // The currently active message pipe to the renderer PhishingDetector.
  mojo::Remote<mojom::PhishingDetector> phishing_detector_;

  // Max number of ips we save for each browse
  static const size_t kMaxIPsPerBrowse;
  // Max number of urls we report for each malware IP.
  static const size_t kMaxUrlsPerIP;

  bool should_extract_malware_features_;
  bool should_classify_for_malware_;
  bool pageload_complete_;

  // Unique page ID of the most recent unsafe site that was loaded in this tab
  // as well as the UnsafeResource.
  int unsafe_unique_page_id_;
  std::unique_ptr<security_interstitials::UnsafeResource> unsafe_resource_;

  base::WeakPtrFactory<ClientSideDetectionHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHost);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
