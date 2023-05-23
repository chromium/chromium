// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace testing {
class MatchResultListener;
}

constexpr char kStorageAccessScript[] = R"(
    async function accessDatabase() {
      var my_db = openDatabase('my_db', '1.0', 'description', 1024);
      var num_rows;
      await new Promise((resolve, reject) => {
        my_db.transaction((tx) => {
          tx.executeSql('CREATE TABLE IF NOT EXISTS tbl (id unique, data)');
          tx.executeSql('INSERT INTO tbl (id, data) VALUES (1, "foo")');
          tx.executeSql('SELECT * FROM tbl', [], (tx, results) => {
            num_rows = results.rows.length;
          });
        }, reject, resolve);
      });
      if(num_rows <= 0) {throw new Error('Failed to access!')}
    }

    function accessLocalStorage() {
      localStorage.setItem('foo', 'bar');
      return localStorage.getItem('foo');
    }

    function accessSessionStorage() {
      sessionStorage.setItem('foo', 'bar');
      return sessionStorage.getItem('foo') == 'bar';
    }

    async function accessFileSystem() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(TEMPORARY, 1024, resolve, reject);
      });
      return new Promise((resolve, reject) => {
        fs.root.getFile('foo.txt', {create: true, exclusive: true}, resolve,
          reject);
      });
    }

    function accessIndexedDB() {
      var request = indexedDB.open('my_db', 2);

      request.onupgradeneeded = () => {
        request.result.createObjectStore('store');
      }
      return new Promise((resolve) => {
        request.onsuccess = () => {
          request.result.close();
          resolve(true);
        }
        request.onerror = () => {throw new Error('Failed to access!')}
      });
    }

    function accessCache() {
      return caches.open("cache")
      .then((cache) => cache.put("/foo", new Response("bar")))
      .then(() => true)
      .catch(() => {throw new Error('Failed to access!')});
    }

    // Placeholder for execution statement.
    access%s();
  )";

using StateForURLCallback = base::OnceCallback<void(DIPSState)>;

class URLCookieAccessObserver : public content::WebContentsObserver {
 public:
  URLCookieAccessObserver(content::WebContents* web_contents,
                          const GURL& url,
                          CookieOperation access_type);

  void Wait();

  bool CookieAccessedInPrimaryPage() const;

 private:
  // WebContentsObserver overrides
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  GURL url_;
  CookieOperation access_type_;
  bool cookie_accessed_in_primary_page_ = false;
  base::RunLoop run_loop_;
};

class FrameCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit FrameCookieAccessObserver(
      content::WebContents* web_contents,
      content::RenderFrameHost* render_frame_host,
      CookieOperation access_type);

  // Wait until the frame accesses cookies.
  void Wait();

  // WebContentsObserver override
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;

 private:
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  CookieOperation access_type_;
  base::RunLoop run_loop_;
};

class RedirectChainObserver : public DIPSService::Observer {
 public:
  explicit RedirectChainObserver(DIPSService* service, GURL final_url);
  ~RedirectChainObserver() override;

  void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) override;

  void Wait();

  size_t handle_call_count = 0;

 private:
  GURL final_url_;
  base::RunLoop run_loop_;
  base::ScopedObservation<DIPSService, Observer> obs_{this};
};

class UserActivationObserver : public content::WebContentsObserver {
 public:
  explicit UserActivationObserver(content::WebContents* web_contents,
                                  content::RenderFrameHost* render_frame_host);

  // Wait until the frame receives user activation.
  void Wait();

 private:
  // WebContentsObserver override
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;

  raw_ptr<content::RenderFrameHost> const render_frame_host_;
  base::RunLoop run_loop_;
};

// Checks that the URLs associated with the UKM entries with the given name are
// as expected. Sorts the URLs so order doesn't matter.
//
// Example usage:
//
// EXPECT_THAT(ukm_recorder, EntryUrlsAre(entry_name, {url1, url2, url3}));
class EntryUrlsAre {
 public:
  using is_gtest_matcher = void;
  EntryUrlsAre(std::string entry_name, std::vector<std::string> urls);
  EntryUrlsAre(const EntryUrlsAre&);
  EntryUrlsAre(EntryUrlsAre&&);
  ~EntryUrlsAre();

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  bool MatchAndExplain(const ukm::TestUkmRecorder& ukm_recorder,
                       testing::MatchResultListener* result_listener) const;

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  std::string entry_name_;
  std::vector<std::string> expected_urls_;
};

// Enables or disables a base::Feature.
class ScopedInitFeature {
 public:
  explicit ScopedInitFeature(const base::Feature& feature,
                             bool enable,
                             const base::FieldTrialParams& params);

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables/disables the DIPS Feature and updates the ProfileSelections of
// DIPSServiceFactory and DIPSCleanupServiceFactory to match.
class ScopedInitDIPSFeature {
 public:
  explicit ScopedInitDIPSFeature(bool enable,
                                 const base::FieldTrialParams& params = {});

 private:
  ScopedInitFeature init_feature_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_for_dips_service_;
  profiles::testing::ScopedProfileSelectionsForFactoryTesting
      override_profile_selections_for_dips_cleanup_service_;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
