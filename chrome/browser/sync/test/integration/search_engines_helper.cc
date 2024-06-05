// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/search_engines_helper.h"

#include <stddef.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/search_engines/template_url.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using sync_datatype_helper::test;

using ::testing::Contains;
using ::testing::Key;
using ::testing::Not;

GUIDToTURLMap CreateGUIDToTURLMap(TemplateURLService* service) {
  GUIDToTURLMap map;
  for (TemplateURL* turl : service->GetTemplateURLs()) {
    EXPECT_THAT(map, Not(Contains(Key(turl->sync_guid()))))
        << "Found two template URLs with same GUID";
    map[turl->sync_guid()] = turl;
  }
  return map;
}

std::string GetTURLInfoString(const TemplateURL& turl) {
  return "TemplateURL: shortname: " + base::UTF16ToUTF8(turl.short_name()) +
         " keyword: " + base::UTF16ToUTF8(turl.keyword()) +
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

bool ServicesMatch(int profile_a, int profile_b, std::ostream* os) {
  TemplateURLService* service_a =
      search_engines_helper::GetServiceForBrowserContext(profile_a);
  TemplateURLService* service_b =
      search_engines_helper::GetServiceForBrowserContext(profile_b);

  // Services that have synced should have identical TURLs, including the GUIDs.
  // Make sure we compare those fields in addition to the user-visible fields.
  GUIDToTURLMap a_turls = CreateGUIDToTURLMap(service_a);
  GUIDToTURLMap b_turls = CreateGUIDToTURLMap(service_b);

  if (a_turls.size() != b_turls.size()) {
    *os << "Service a and b do not match in size: " << a_turls.size() << " vs "
        << b_turls.size() << " respectively.";
    return false;
  }

  for (const auto& [guid, a_turl] : a_turls) {
    if (b_turls.find(guid) == b_turls.end()) {
      *os << "TURL GUID from a not found in b's TURLs: " << guid;
      return false;
    }
    if (!TURLsMatch(*b_turls[guid], *a_turl)) {
      return false;
    }
  }

  const TemplateURL* default_a = service_a->GetDefaultSearchProvider();
  const TemplateURL* default_b = service_b->GetDefaultSearchProvider();
  if (!TURLsMatch(*default_a, *default_b)) {
    *os << "Default search providers do not match: A's default: "
        << default_a->keyword() << " B's default: " << default_b->keyword();
    return false;
  } else {
    *os << "A had default with URL: " << default_a->url()
        << " and keyword: " << default_a->keyword();
  }

  return true;
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

  for (TemplateURL* verifier_turl : verifier_turls) {
    const TemplateURL* other_turl =
        other->GetTemplateURLForKeyword(verifier_turl->keyword());

    if (!other_turl) {
      DVLOG(1) << "The other service did not contain a TURL with keyword: "
               << verifier_turl->keyword();
      return false;
    }
    if (!TURLsMatch(*verifier_turl, *other_turl)) {
      return false;
    }
  }

  return true;
}

bool AllServicesMatch() {
  return AllServicesMatch(&VLOG_STREAM(1));
}

bool AllServicesMatch(std::ostream* os) {
  // Use 0 as the baseline.
  if (test()->UseVerifier() && !ServiceMatchesVerifier(0)) {
    *os << "TemplateURLService 0 does not match verifier.";
    return false;
  }

  for (int it = 1; it < test()->num_clients(); ++it) {
    if (!ServicesMatch(0, it, os)) {
      *os << "TemplateURLService " << it << " does not match with "
          << "service 0.";
      return false;
    }
  }
  return true;
}

TemplateURLBuilder::TemplateURLBuilder(const std::string& keyword) {
  data_.SetShortName(base::UTF8ToUTF16(keyword));
  data_.SetKeyword(base::UTF8ToUTF16(keyword));
  data_.SetURL(base::StringPrintf("http://www.test-%s.com/", keyword.c_str()));
  data_.favicon_url = GURL("http://favicon.url");
  data_.safe_for_autoreplace = true;
  data_.date_created = base::Time::FromTimeT(100);
  data_.last_modified = base::Time::FromTimeT(100);
  data_.prepopulate_id = 999999;

  // Produce a GUID deterministically from |keyword|.
  std::string hex_encoded_hash =
      base::HexEncode(base::SHA1Hash(base::as_byte_span(keyword)));
  hex_encoded_hash.resize(12);
  data_.sync_guid =
      base::StrCat({"12345678-0000-4000-8000-", hex_encoded_hash});
  DCHECK(base::Uuid::ParseCaseInsensitive(data_.sync_guid).is_valid());
}

TemplateURLBuilder::~TemplateURLBuilder() = default;

std::unique_ptr<TemplateURL> TemplateURLBuilder::Build() {
  return std::make_unique<TemplateURL>(data_);
}

void AddSearchEngine(int profile_index, const std::string& keyword) {
  Profile* profile = test()->GetProfile(profile_index);
  TemplateURLBuilder builder(keyword);
  TemplateURLServiceFactory::GetForProfile(profile)->Add(builder.Build());
  if (test()->UseVerifier()) {
    GetVerifierService()->Add(builder.Build());
  }
}

void EditSearchEngine(int profile_index,
                      const std::string& keyword,
                      const std::u16string& short_name,
                      const std::string& new_keyword,
                      const std::string& url) {
  ASSERT_FALSE(url.empty());
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl =
      service->GetTemplateURLForKeyword(base::UTF8ToUTF16(keyword));
  EXPECT_TRUE(turl);
  ASSERT_FALSE(new_keyword.empty());
  service->ResetTemplateURL(turl, short_name, base::UTF8ToUTF16(new_keyword),
                            url);
  // Make sure we do the same on the verifier.
  if (test()->UseVerifier()) {
    TemplateURL* verifier_turl = GetVerifierService()->GetTemplateURLForKeyword(
        base::UTF8ToUTF16(keyword));
    EXPECT_TRUE(verifier_turl);
    GetVerifierService()->ResetTemplateURL(verifier_turl, short_name,
                                           base::UTF8ToUTF16(new_keyword), url);
  }
}

void DeleteSearchEngine(int profile_index, const std::string& keyword) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl =
      service->GetTemplateURLForKeyword(base::UTF8ToUTF16(keyword));
  EXPECT_TRUE(turl);
  service->Remove(turl);
  // Make sure we do the same on the verifier.
  if (test()->UseVerifier()) {
    TemplateURL* verifier_turl = GetVerifierService()->GetTemplateURLForKeyword(
        base::UTF8ToUTF16(keyword));
    EXPECT_TRUE(verifier_turl);
    GetVerifierService()->Remove(verifier_turl);
  }
}

void ChangeDefaultSearchProvider(int profile_index,
                                 const std::string& keyword) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl =
      service->GetTemplateURLForKeyword(base::UTF8ToUTF16(keyword));
  ASSERT_TRUE(turl);
  service->SetUserSelectedDefaultSearchProvider(turl);
  if (test()->UseVerifier()) {
    TemplateURL* verifier_turl = GetVerifierService()->GetTemplateURLForKeyword(
        base::UTF8ToUTF16(keyword));
    ASSERT_TRUE(verifier_turl);
    GetVerifierService()->SetUserSelectedDefaultSearchProvider(verifier_turl);
  }
}

bool HasSearchEngine(int profile_index, const std::string& keyword) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  TemplateURL* turl =
      service->GetTemplateURLForKeyword(base::UTF8ToUTF16(keyword));
  return turl != nullptr;
}

std::string GetDefaultSearchEngineKeyword(int profile_index) {
  TemplateURLService* service = GetServiceForBrowserContext(profile_index);
  return base::UTF16ToUTF8(service->GetDefaultSearchProvider()->keyword());
}

SearchEnginesMatchChecker::SearchEnginesMatchChecker() {
  if (test()->UseVerifier()) {
    observations_.AddObservation(GetVerifierService());
  }

  for (int i = 0; i < test()->num_clients(); ++i) {
    observations_.AddObservation(GetServiceForBrowserContext(i));
  }
}

SearchEnginesMatchChecker::~SearchEnginesMatchChecker() = default;

bool SearchEnginesMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  return AllServicesMatch(os);
}

void SearchEnginesMatchChecker::OnTemplateURLServiceChanged() {
  CheckExitCondition();
}

HasSearchEngineChecker::HasSearchEngineChecker(int profile_index,
                                               const std::string& keyword)
    : service_(GetServiceForBrowserContext(profile_index)),
      keyword_(base::UTF8ToUTF16(keyword)) {
  observations_.AddObservation(service_.get());
}

HasSearchEngineChecker::~HasSearchEngineChecker() = default;

bool HasSearchEngineChecker::IsExitConditionSatisfied(std::ostream* os) {
  return service_->GetTemplateURLForKeyword(keyword_) != nullptr;
}

void HasSearchEngineChecker::OnTemplateURLServiceChanged() {
  CheckExitCondition();
}

}  // namespace search_engines_helper
