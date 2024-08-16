// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/container_app/container_app_tab_helper.h"

#include "base/hash/md5_constexpr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// Returns pages mapped to their MD5 hashes.
std::map<uint64_t, ContainerAppTabHelper::Page>* GetMD5PageHashes() {
  using Page = ContainerAppTabHelper::Page;
  static base::NoDestructor<std::map<uint64_t, Page>> md5_page_hashes(
      {{15434391541687473744u, Page::kCongratulations},
       {2639084485652816410u, Page::kDebug},
       {8933819972841556021u, Page::kDebug},
       {11887379483153592206u, Page::kDebug},
       {6579551706563083045u, Page::kOffer},
       {9605163350832310418u, Page::kTermsAndConditions},
       {14050260147306734198u, Page::kTermsAndConditions},
       {18084016612939108325u, Page::kTermsAndConditions}});
  return md5_page_hashes.get();
}

// Returns whether the specified `web_contents` is off the record.
bool IsOffTheRecord(content::WebContents* web_contents) {
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  return browser_context && browser_context->IsOffTheRecord();
}

}  // namespace

// ContainerAppTabHelper -------------------------------------------------------

ContainerAppTabHelper::ContainerAppTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContainerAppTabHelper>(*web_contents) {}

ContainerAppTabHelper::~ContainerAppTabHelper() = default;

// static
void ContainerAppTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (chromeos::features::IsContainerAppPreinstallEnabled() &&
      !IsOffTheRecord(web_contents)) {
    ContainerAppTabHelper::CreateForWebContents(web_contents);
  }
}

// static
base::AutoReset<std::map<uint64_t, ContainerAppTabHelper::Page>>
ContainerAppTabHelper::SetPageUrlsForTesting(std::map<GURL, Page> page_urls) {
  std::map<uint64_t, Page> md5_page_hashes;
  for (const auto& [url, page] : page_urls) {
    md5_page_hashes.emplace(base::MD5Hash64Constexpr(url.spec()), page);
  }
  return {GetMD5PageHashes(), std::move(md5_page_hashes)};
}

void ContainerAppTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  const std::map<uint64_t, ContainerAppTabHelper::Page>* md5_page_hashes =
      GetMD5PageHashes();

  // Check for exact page match.
  auto it = md5_page_hashes->find(
      base::MD5Hash64Constexpr(navigation_handle->GetURL().spec()));

  // Check for page match w/o filename.
  if (it == md5_page_hashes->end()) {
    it = md5_page_hashes->find(base::MD5Hash64Constexpr(
        navigation_handle->GetURL().GetWithoutFilename().spec()));
  }

  // Record page visit.
  if (it != md5_page_hashes->end()) {
    base::UmaHistogramEnumeration("Ash.ContainerApp.Page.Visit",
                                  /*page=*/it->second);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContainerAppTabHelper);
