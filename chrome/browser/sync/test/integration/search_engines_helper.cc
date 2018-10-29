// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/search_engines_helper.h"

#include <stddef.h>

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

using sync_datatype_helper::test;

namespace {

GUIDToTURLMap CreateGUIDToTURLMap(TemplateURLService* service) {
  GUIDToTURLMap map;
  for (TemplateURL* turl : service->GetTemplateURLs()) {
    EXPECT_TRUE(map.find(turl->sync_guid()) == map.end());
    map[turl->sync_guid()] = turl;
  }
  return map;
}

std::string GetTURLInfoString(const TemplateURL& turl) {
  return "TemplateURL: shortname: " + base::UTF16ToASCII(turl.short_name()) +
         " keyword: " + base::UTF16ToASCII(turl.keyword()) +
         " url: " + turl.url();
}

bool TURLsMatch(const TemplateURL& turl1, const TemplateURL& turl2) {
  bool result = (turl1.url() == turl2.url()) &&
                (turl1.keyword() == turl2.keyword()) &&
                (turl1.short_name() == turl2.short_name());

  // Print some useful debug info.
  if (!result) {
    DVLOG(1) << "TemplateURLs did not match: " << GetTURLInfoString(turl1)
             << " vs " << GetTURLInfoString(turl2);
  }

  return result;
}

bool ServicesMatch(int profile_a, int profile_b) {
  TemplateURLService* service_a =
      search_engines_helper::GetServiceForBrowserContext(profile_a);
  TemplateURLService* service_b =
      search_engines_helper::GetServiceForBrowserContext(profile_b);

  // Services that have synced should have identical TURLs, including the GUIDs.
  // Make sure we compare those fields in addition to the user-visible fields.
  GUIDToTURLMap a_turls = CreateGUIDToTURLMap(service_a);
  GUIDToTURLMap b_turls = CreateGUIDToTURLMap(service_b);

  if (a_turls.size() != b_turls.size()) {
    DVLOG(1) << "Service a and b do not match in size: " << a_turls.size()
             << " vs " << b_turls.size() << " respectively.";
    return false;
  }

  for (auto it = a_turls.begin(); it != a_turls.end(); ++it) {
    if (b_turls.find(it->first) == b_turls.end()) {
      DVLOG(1) << "TURL GUID from a not found in b's TURLs: " << it->first;
      return false;
    }
    if (!TURLsMatch(*b_turls[it->first], *it->second))
      return false;
  }

  const TemplateURL* default_a = service_a->GetDefaultSearchProvider();
  const TemplateURL* default_b = service_b->GetDefaultSearchProvider();
  if (!TURLsMatch(*default_a, *default_b)) {
    DVLOG(1) << "Default search providers do not match: A's default: "
             << default_a->keyword()
             << " B's default: " << default_b->keyword();
    return false;
  } else {
    DVLOG(1) << "A had default with URL: " << default_a->url()
             << " and keyword: " << default_a->keyword();
  }

  return true;
}

// Convenience helper for consistently generating the same keyword for a given
// seed.
base::string16 CreateKeyword(int seed) {
  return base::ASCIIToUTF16(base::StringPrintf("test%d", seed));
}

}  // namespace

namespace search_engines_helper {

TemplateURLService* GetServiceForBrowserContext(int profile_index) {
  return TemplateURLServiceFactory::GetForProfile(
      test()->GetProfile(profile_index));
}

TemplateURLService* GetVerifierService() {
  return TemplateURLServiceFactory::GetForProfile(test()->verifier());
}

bool ServiceMatchesVerifier(int profile_index) {
  TemplateURLService* verifier = GetVerifierService();
  TemplateURLService* other = GetServiceForBrowserContext(profile_index);

  TemplateURLService::TemplateURLVector verifier_turls =
      verifier->GetTemplateURLs();
  if (verifier_turls.size() != other->GetTemplateURLs().size()) {
    DVLOG(1) << "Verifier and other service have a different count of TURLs: "
             << verifier_turls.size() << " vs "
             << other->GetTemplateURLs().size() << " respectively.";
    return false;
  }

  for (size_t i = 0; i < verifier_turls.size(); ++i) {
    const TemplateURL& verifier_turl = *verifier_turls.at(i);
    const TemplateURL* other_turl =
        other->GetTemplateURLForKeyword(verifier_turl.keyword());

    if (!other_turl) {
      DVLOG(1) << "The other service did not contain a TURL with keyword: "
               << verifier_turl.keyword();
      return false;
    }
    if (!TURLsMatch(verifier_turl, *other_turl))
      return false;
  }

  return true;
}

bool AllServicesMatch() {
  // Use 0 as the baseline.
  if (test()->use_verifier() && !ServiceMatchesVerifier(0)) {
    DVLOG(1) << "TemplateURLService 0 does not match verifier.";
    return false;
  }

  for (int it = 1; it < test()->num_clients(); ++it) {
    if (!ServicesMatch(0, it)) {
      DVLOG(1) << "TemplateURLService " << it << " does not match with "
               << "service 0.";
      return false;
    }
  }
  return true;
}

std::unique_ptr<TemplateURL> CreateTestTemplateURL(Profile* profile, int seed) {
  return CreateTestTemplateURL(profile, seed, CreateKeyword(seed),
                               base::StringPrintf("0000-0000-0000-%04d", seed));
}

std::unique_ptr<TemplateURL> CreateTestTemplateURL(
    Profile* profile,
    int seed,
    const base::string16& keyword,
    const std::string& sync_guid) {
  return CreateTestTemplateURL(profile, seed, keyword,
      base::StringPrintf("http://www.test%d.com/", seed), sync_guid);
}

std::unique_ptr<TemplateURL> CreateTestTemplateURL(
    Profile* profile,
    int seed,
    const base::string16& keyword,
    const std::string& url,
    const std::string& sync_guid) {
  TemplateURLData data;
  data.SetShortName(CreateKeyword(seed));
  data.SetKeyword(keyword);
  data.SetURL(url);
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = true;
  data.date_created = base::Time::FromTimeT(100);
  data.last_modified = base::Time::FromTimeT(100);
  data.prepopulate_id = 999999;
  data.sync_guid = sync_guid;
  return std::make_unique<TemplateURL>(data);
}

void AddSearchEngine(int profile_index, int seed) {
  Profile* profile = test()->GetProfile(profile_index);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(
      CreateTestTemplateURL(profile, seed));
  if (test()->use_verifier())
    GetVerifierService()->Add(CreateTestTemplateURL(profile, seed));
}

void EditSearchEngine(int profile_index,
                      const base::string16& keyword,
                      const base::string16& short_name,
                      const base::string16& new_keyword,
                      const std::string& url) {
  ASSERT_FALSE(url.empty());
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl = service->GetTemplateURLForKeyword(keyword);
  EXPECT_TRUE(turl);
  ASSERT_FALSE(new_keyword.empty());
  service->ResetTemplateURL(turl, short_name, new_keyword, url);
  // Make sure we do the same on the verifier.
  if (test()->use_verifier()) {
    TemplateURL* verifier_turl =
        GetVerifierService()->GetTemplateURLForKeyword(keyword);
    EXPECT_TRUE(verifier_turl);
    GetVerifierService()->ResetTemplateURL(verifier_turl, short_name,
                                           new_keyword, url);
  }
}

void DeleteSearchEngineBySeed(int profile_index, int seed) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  base::string16 keyword(CreateKeyword(seed));
  TemplateURL* turl = service->GetTemplateURLForKeyword(keyword);
  EXPECT_TRUE(turl);
  service->Remove(turl);
  // Make sure we do the same on the verifier.
  if (test()->use_verifier()) {
    TemplateURL* verifier_turl =
        GetVerifierService()->GetTemplateURLForKeyword(keyword);
    EXPECT_TRUE(verifier_turl);
    GetVerifierService()->Remove(verifier_turl);
  }
}

void ChangeDefaultSearchProvider(int profile_index, int seed) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl = service->GetTemplateURLForKeyword(CreateKeyword(seed));
  ASSERT_TRUE(turl);
  service->SetUserSelectedDefaultSearchProvider(turl);
  if (test()->use_verifier()) {
    TemplateURL* verifier_turl =
        GetVerifierService()->GetTemplateURLForKeyword(CreateKeyword(seed));
    ASSERT_TRUE(verifier_turl);
    GetVerifierService()->SetUserSelectedDefaultSearchProvider(verifier_turl);
  }
}

bool HasSearchEngine(int profile_index, int seed) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  base::string16 keyword(CreateKeyword(seed));
  TemplateURL* turl = service->GetTemplateURLForKeyword(keyword);
  return turl != nullptr;
}

}  // namespace search_engines_helper

SearchEnginesMatchChecker::SearchEnginesMatchChecker()
    : AwaitMatchStatusChangeChecker(
          base::Bind(search_engines_helper::AllServicesMatch),
          "All search engines match") {}
