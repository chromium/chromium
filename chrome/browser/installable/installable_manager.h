// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/installable/installable_task_queue.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// This class is responsible for fetching the resources required to check and
// install a site.
class InstallableManager
    : public content::ServiceWorkerContextObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<InstallableManager> {
 public:
  explicit InstallableManager(content::WebContents* web_contents);
  ~InstallableManager() override;

  // Returns the minimum icon size in pixels for a site to be installable.
  static int GetMinimumIconSizeInPx();

  // Returns true if the overall security state of |web_contents| is sufficient
  // to be considered installable.
  static bool IsContentSecure(content::WebContents* web_contents);

  // Returns true for localhost and URLs that have been explicitly marked as
  // secure via a flag.
  static bool IsOriginConsideredSecure(const GURL& url);

  // Get the installable data, fetching the resources specified in |params|.
  // |callback| is invoked synchronously (i.e. not via PostTask on the UI thread
  // when the data is ready; the synchronous execution ensures that the
  // references |callback| receives in its InstallableData argument are valid.
  //
  // |callback| may never be invoked if |params.wait_for_worker| is true, or if
  // the user navigates the page before fetching is complete.
  //
  // Calls requesting data that has already been fetched will return the cached
  // data.
  virtual void GetData(const InstallableParams& params,
                       InstallableCallback callback);

  // Runs the full installability check, and when finished, runs |callback|
  // passing a list of human-readable strings describing the errors encountered
  // during the run. The list is empty if no errors were encountered.
  void GetAllErrors(
      base::OnceCallback<void(std::vector<std::string> errors)> callback);

 protected:
  // For mocking in tests.
  virtual void OnWaitingForServiceWorker() {}
  virtual void OnResetData() {}

 private:
  friend class content::WebContentsUserData<InstallableManager>;
  friend class AddToHomescreenDataFetcherTest;
  friend class InstallableManagerBrowserTest;
  friend class InstallableManagerUnitTest;
  friend class TestInstallableManager;
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManagerBeginsInEmptyState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, ManagerInIncognito);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, CheckWebapp);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManifestUrlChangeFlushesState);

  using IconPurpose = blink::Manifest::ImageResource::Purpose;

  struct EligiblityProperty {
    EligiblityProperty();
    ~EligiblityProperty();

    std::vector<InstallableStatusCode> errors;
    bool fetched = false;
  };

  struct ManifestProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    GURL url;
    blink::Manifest manifest;
    bool fetched = false;
  };

  struct ValidManifestProperty {
    ValidManifestProperty();
    ~ValidManifestProperty();

    std::vector<InstallableStatusCode> errors;
    bool is_valid = false;
    bool fetched = false;
  };

  struct ServiceWorkerProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    bool has_worker = false;
    bool is_waiting = false;
    bool fetched = false;
  };

  struct IconProperty {
    IconProperty();
    IconProperty(IconProperty&& other);
    ~IconProperty();
    IconProperty& operator=(IconProperty&& other);

    InstallableStatusCode error;
    GURL url;
    std::unique_ptr<SkBitmap> icon;
    bool fetched;

   private:
    // This class contains a std::unique_ptr and therefore must be move-only.
    DISALLOW_COPY_AND_ASSIGN(IconProperty);
  };

  // Returns true if |purpose| matches any fetched icon, or false if no icon has
  // been requested yet or there is no match.
  bool IsIconFetched(const IconPurpose purpose) const;
  bool IsPrimaryIconFetched(const InstallableParams& params) const;

  // Sets the icon matching |purpose| as fetched.
  void SetIconFetched(const IconPurpose purpose);

  // Gets the purpose of the icon to use as a primary icon.
  IconPurpose GetPrimaryIconPurpose(const InstallableParams& params) const;

  // Returns a vector with all errors encountered for the resources requested in
  // |params|, or an empty vector if there is no error.
  std::vector<InstallableStatusCode> GetErrors(const InstallableParams& params);

  // Gets/sets parts of particular properties. Exposed for testing.
  InstallableStatusCode eligibility_error() const;
  InstallableStatusCode manifest_error() const;
  InstallableStatusCode valid_manifest_error() const;
  void set_valid_manifest_error(InstallableStatusCode error_code);
  InstallableStatusCode worker_error() const;
  InstallableStatusCode icon_error(const IconPurpose purpose);
  GURL& icon_url(const IconPurpose purpose);
  const SkBitmap* icon(const IconPurpose purpose);

  // Returns the WebContents to which this object is attached, or nullptr if the
  // WebContents doesn't exist or is currently being destroyed.
  content::WebContents* GetWebContents();

  // Returns true if |params| requires no more work to be done.
  bool IsComplete(const InstallableParams& params) const;

  // Resets members to empty and removes all queued tasks.
  // Called when navigating to a new page or if the WebContents is destroyed
  // whilst waiting for a callback.
  void Reset();

  // Sets the fetched bit on the installable and icon subtasks.
  // Called if no manifest (or an empty manifest) was fetched from the site.
  void SetManifestDependentTasksComplete();

  // Methods coordinating and dispatching work for the current task.
  void CleanupAndStartNextTask();
  void RunCallback(InstallableTask task,
                   std::vector<InstallableStatusCode> errors);
  void WorkOnTask();

  // Data retrieval methods.
  void CheckEligiblity();
  void FetchManifest();
  void OnDidGetManifest(const GURL& manifest_url,
                        const blink::Manifest& manifest);

  void CheckManifestValid(bool check_webapp_manifest_display,
                          bool prefer_maskable_icon);
  bool IsManifestValidForWebApp(const blink::Manifest& manifest,
                                bool check_webapp_manifest_display,
                                bool prefer_maskable_icon);
  void CheckServiceWorker();
  void OnDidCheckHasServiceWorker(content::ServiceWorkerCapability capability);

  void CheckAndFetchBestIcon(int ideal_icon_size_in_px,
                             int minimum_icon_size_in_px,
                             const IconPurpose purpose);
  void OnIconFetched(const GURL icon_url,
                     const IconPurpose purpose,
                     const SkBitmap& bitmap);

  // content::ServiceWorkerContextObserver overrides
  void OnRegistrationCompleted(const GURL& pattern) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  // content::WebContentsObserver overrides
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidUpdateWebManifestURL(
      const base::Optional<GURL>& manifest_url) override;
  void WebContentsDestroyed() override;

  const GURL& manifest_url() const;
  const blink::Manifest& manifest() const;
  bool valid_manifest();
  bool has_worker();

  InstallableTaskQueue task_queue_;

  // Installable properties cached on this object.
  std::unique_ptr<EligiblityProperty> eligibility_;
  std::unique_ptr<ManifestProperty> manifest_;
  std::unique_ptr<ValidManifestProperty> valid_manifest_;
  std::unique_ptr<ServiceWorkerProperty> worker_;
  std::map<IconPurpose, IconProperty> icons_;

  // Owned by the storage partition attached to the content::WebContents which
  // this object is scoped to.
  content::ServiceWorkerContext* service_worker_context_;

  // True if for the current page load we have in queue or completed a task
  // which queries the full PWA parameters.
  bool has_pwa_check_;

  base::WeakPtrFactory<InstallableManager> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(InstallableManager);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
