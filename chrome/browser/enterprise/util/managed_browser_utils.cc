// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/enterprise/util/jni_headers/ManagedBrowserUtils_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/managed_ui.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace chrome {
namespace enterprise_util {

namespace {

// Returns client certificate auto-selection filters configured for the given
// URL in |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting. The
// format of the returned filters corresponds to the "filter" property of the
// AutoSelectCertificateForUrls policy as documented at policy_templates.json.
base::Value::List GetCertAutoSelectionFilters(Profile* profile,
                                              const GURL& requesting_url) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  base::Value setting = host_content_settings_map->GetWebsiteSetting(
      requesting_url, requesting_url,
      ContentSettingsType::AUTO_SELECT_CERTIFICATE, nullptr);

  if (!setting.is_dict())
    return {};

  base::Value::List* filters = setting.GetDict().FindList("filters");
  if (!filters) {
    // |setting_dict| has the wrong format (e.g. single filter instead of a
    // list of filters). This content setting is only provided by
    // the |PolicyProvider|, which should always set it to a valid format.
    // Therefore, delete the invalid value.
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        requesting_url, requesting_url,
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, base::Value());
    return {};
  }
  return std::move(*filters);
}

// Returns whether the client certificate matches any of the auto-selection
// filters. Returns false when there's no valid filter.
bool CertMatchesSelectionFilters(
    const net::ClientCertIdentity& client_cert,
    const base::Value::List& auto_selection_filters) {
  for (const auto& filter : auto_selection_filters) {
    if (!filter.is_dict()) {
      // The filter has a wrong format, so ignore it. Note that reporting of
      // schema violations, like this, to UI is already implemented in the
      // policy handler - see configuration_policy_handler_list_factory.cc.
      continue;
    }
    auto issuer_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("ISSUER"), "CN", "L",
                              "O", "OU");
    auto subject_pattern = certificate_matching::CertificatePrincipalPattern::
        ParseFromOptionalDict(filter.GetDict().FindDict("SUBJECT"), "CN", "L",
                              "O", "OU");

    if (issuer_pattern.Matches(client_cert.certificate()->issuer()) &&
        subject_pattern.Matches(client_cert.certificate()->subject())) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsBrowserManaged(Profile* profile) {
  DCHECK(profile);
  return policy::ManagementServiceFactory::GetForProfile(profile)->IsManaged();
}

std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(email);
}

void AutoSelectCertificates(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList client_certs,
    net::ClientCertIdentityList* matching_client_certs,
    net::ClientCertIdentityList* nonmatching_client_certs) {
  matching_client_certs->clear();
  nonmatching_client_certs->clear();
  const base::Value::List auto_selection_filters =
      GetCertAutoSelectionFilters(profile, requesting_url);
  for (auto& client_cert : client_certs) {
    if (CertMatchesSelectionFilters(*client_cert, auto_selection_filters))
      matching_client_certs->push_back(std::move(client_cert));
    else
      nonmatching_client_certs->push_back(std::move(client_cert));
  }
}

bool IsMachinePolicyPref(const std::string& pref_name) {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(pref_name);

  return pref && pref->IsManaged();
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedAutoSelectCertificateForUrls);
}

void SetUserAcceptedAccountManagement(Profile* profile, bool accepted) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry)
    entry->SetUserAcceptedAccountManagement(accepted);
}

bool UserAcceptedAccountManagement(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return false;
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->UserAcceptedAccountManagement();
}

bool ProfileCanBeManaged(Profile* profile) {
  // Some tests do not have a profile manager.
  if (!g_browser_process->profile_manager())
    return false;
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->CanBeManaged();
}

bool IsKnownConsumerDomain(const std::string& email_domain) {
  // List of consumer-only domains from the server side logic. See
  // `KNOWN_INVALID_DOMAINS` from GetAgencySignupStateProducerModule.java.
  // Sharing this in open source Chromium code was green-lighted according to
  // https://chromium-review.googlesource.com/c/chromium/src/+/2945029/comment/d8731200_4064534e/
  static constexpr auto kKnownConsumerDomains =
      base::MakeFixedFlatSet<base::StringPiece>({"123mail.org",
                                                 "150mail.com",
                                                 "150ml.com",
                                                 "16mail.com",
                                                 "2-mail.com",
                                                 "2trom.com",
                                                 "4email.net",
                                                 "50mail.com",
                                                 "aapt.net.au",
                                                 "accountant.com",
                                                 "acdcfan.com",
                                                 "activist.com",
                                                 "adam.com.au",
                                                 "adexec.com",
                                                 "africamail.com",
                                                 "aircraftmail.com",
                                                 "airpost.net",
                                                 "allergist.com",
                                                 "allmail.net",
                                                 "alumni.com",
                                                 "alumnidirector.com",
                                                 "angelic.com",
                                                 "anonymous.to",
                                                 "aol.com",
                                                 "appraiser.net",
                                                 "archaeologist.com",
                                                 "arcticmail.com",
                                                 "artlover.com",
                                                 "asia-mail.com",
                                                 "asia.com",
                                                 "atheist.com",
                                                 "auctioneer.net",
                                                 "australiamail.com",
                                                 "bartender.net",
                                                 "bellair.net",
                                                 "berlin.com",
                                                 "bestmail.us",
                                                 "bigpond.com",
                                                 "bigpond.com.au",
                                                 "bigpond.net.au",
                                                 "bikerider.com",
                                                 "birdlover.com",
                                                 "blader.com",
                                                 "boardermail.com",
                                                 "brazilmail.com",
                                                 "brew-master.com",  // nocheck
                                                 "brew-meister.com",
                                                 "bsdmail.com",
                                                 "californiamail.com",
                                                 "cash4u.com",
                                                 "catlover.com",
                                                 "cheerful.com",
                                                 "chef.net",
                                                 "chemist.com",
                                                 "chinamail.com",
                                                 "clerk.com",
                                                 "clubmember.org",
                                                 "cluemail.com",
                                                 "collector.org",
                                                 "columnist.com",
                                                 "comcast.net",
                                                 "comic.com",
                                                 "computer4u.com",
                                                 "consultant.com",
                                                 "contractor.net",
                                                 "coolsite.net",
                                                 "counsellor.com",
                                                 "cutey.com",
                                                 "cyber-wizard.com",
                                                 "cyberdude.com",
                                                 "cybergal.com",
                                                 "cyberservices.com",
                                                 "dallasmail.com",
                                                 "dbzmail.com",
                                                 "deliveryman.com",
                                                 "diplomats.com",
                                                 "disciples.com",
                                                 "discofan.com",
                                                 "disposable.com",
                                                 "dispostable.com",
                                                 "dodo.com.au",
                                                 "doglover.com",
                                                 "doramail.com",
                                                 "dr.com",
                                                 "dublin.com",
                                                 "dutchmail.com",
                                                 "earthlink.net",
                                                 "elitemail.org",
                                                 "elvisfan.com",
                                                 "email.com",
                                                 "emailcorner.net",
                                                 "emailengine.net",
                                                 "emailengine.org",
                                                 "emailgroups.net",
                                                 "emailplus.org",
                                                 "emailuser.net",
                                                 "eml.cc",
                                                 "engineer.com",
                                                 "englandmail.com",
                                                 "europe.com",
                                                 "europemail.com",
                                                 "everymail.net",
                                                 "everyone.net",
                                                 "execs.com",
                                                 "exemail.com.au",
                                                 "f-m.fm",
                                                 "facebook.com",
                                                 "fast-email.com",
                                                 "fast-mail.org",
                                                 "fastem.com",
                                                 "fastemail.us",
                                                 "fastemailer.com",
                                                 "fastest.cc",
                                                 "fastimap.com",
                                                 "fastmail.cn",
                                                 "fastmail.co.uk",
                                                 "fastmail.com.au",
                                                 "fastmail.es",
                                                 "fastmail.fm",
                                                 "fastmail.im",
                                                 "fastmail.in",
                                                 "fastmail.jp",
                                                 "fastmail.mx",
                                                 "fastmail.net",
                                                 "fastmail.nl",
                                                 "fastmail.se",
                                                 "fastmail.to",
                                                 "fastmail.tw",
                                                 "fastmail.us",
                                                 "fastmailbox.net",
                                                 "fastmessaging.com",
                                                 "fastservice.com",
                                                 "fea.st",
                                                 "financier.com",
                                                 "fireman.net",
                                                 "flashmail.com",
                                                 "fmail.co.uk",
                                                 "fmailbox.com",
                                                 "fmgirl.com",
                                                 "fmguy.com",
                                                 "ftml.net",
                                                 "galaxyhit.com",
                                                 "gardener.com",
                                                 "geologist.com",
                                                 "germanymail.com",
                                                 "gmail.com",
                                                 "gmx.com",
                                                 "googlemail.com",
                                                 "graduate.org",
                                                 "graphic-designer.com",
                                                 "greenmail.net",
                                                 "groupmail.com",
                                                 "guerillamail.com",
                                                 "h-mail.us",
                                                 "hackermail.com",
                                                 "hailmail.net",
                                                 "hairdresser.net",
                                                 "hilarious.com",
                                                 "hiphopfan.com",
                                                 "homemail.com",
                                                 "hot-shot.com",
                                                 "hotmail.co.uk",
                                                 "hotmail.com",
                                                 "hotmail.fr",
                                                 "hotmail.it",
                                                 "housemail.com",
                                                 "humanoid.net",
                                                 "hushmail.com",
                                                 "icloud.com",
                                                 "iinet.net.au",
                                                 "imap-mail.com",
                                                 "imap.cc",
                                                 "imapmail.org",
                                                 "iname.com",
                                                 "inbox.com",
                                                 "innocent.com",
                                                 "inorbit.com",
                                                 "inoutbox.com",
                                                 "instruction.com",
                                                 "instructor.net",
                                                 "insurer.com",
                                                 "internet-e-mail.com",
                                                 "internet-mail.org",
                                                 "internetemails.net",
                                                 "internetmailing.net",
                                                 "internode.on.net",
                                                 "iprimus.com.au",
                                                 "irelandmail.com",
                                                 "israelmail.com",
                                                 "italymail.com",
                                                 "jetemail.net",
                                                 "job4u.com",
                                                 "journalist.com",
                                                 "justemail.net",
                                                 "keromail.com",
                                                 "kissfans.com",
                                                 "kittymail.com",
                                                 "koreamail.com",
                                                 "lawyer.com",
                                                 "legislator.com",
                                                 "letterboxes.org",
                                                 "linuxmail.org",
                                                 "live.co.uk",
                                                 "live.com",
                                                 "live.com.au",
                                                 "lobbyist.com",
                                                 "lovecat.com",
                                                 "lycos.com",
                                                 "mac.com",
                                                 "madonnafan.com",
                                                 "mail-central.com",
                                                 "mail-me.com",
                                                 "mail-page.com",
                                                 "mail.com",
                                                 "mail.ru",
                                                 "mailandftp.com",
                                                 "mailas.com",
                                                 "mailbolt.com",
                                                 "mailc.net",
                                                 "mailcan.com",
                                                 "mailforce.net",
                                                 "mailftp.com",
                                                 "mailhaven.com",
                                                 "mailinator.com",
                                                 "mailingaddress.org",
                                                 "mailite.com",
                                                 "mailmight.com",
                                                 "mailnew.com",
                                                 "mailsent.net",
                                                 "mailservice.ms",
                                                 "mailup.net",
                                                 "mailworks.org",
                                                 "marchmail.com",
                                                 "me.com",
                                                 "metalfan.com",
                                                 "mexicomail.com",
                                                 "minister.com",
                                                 "ml1.net",
                                                 "mm.st",
                                                 "moscowmail.com",
                                                 "msn.com",
                                                 "munich.com",
                                                 "musician.org",
                                                 "muslim.com",
                                                 "myfastmail.com",
                                                 "mymacmail.com",
                                                 "myself.com",
                                                 "net-shopping.com",
                                                 "netspace.net.au",
                                                 "ninfan.com",
                                                 "nonpartisan.com",
                                                 "nospammail.net",
                                                 "null.net",
                                                 "nycmail.com",
                                                 "oath.com",
                                                 "onebox.com",
                                                 "operamail.com",
                                                 "optician.com",
                                                 "optusnet.com.au",
                                                 "orthodontist.net",
                                                 "outlook.com",
                                                 "ownmail.net",
                                                 "pacific-ocean.com",
                                                 "pacificwest.com",
                                                 "pediatrician.com",
                                                 "petlover.com",
                                                 "petml.com",
                                                 "photographer.net",
                                                 "physicist.net",
                                                 "planetmail.com",
                                                 "planetmail.net",
                                                 "polandmail.com",
                                                 "politician.com",
                                                 "post.com",
                                                 "postinbox.com",
                                                 "postpro.net",
                                                 "presidency.com",
                                                 "priest.com",
                                                 "programmer.net",
                                                 "proinbox.com",
                                                 "promessage.com",
                                                 "protestant.com",
                                                 "publicist.com",
                                                 "qmail.com",
                                                 "qq.com",
                                                 "qualityservice.com",
                                                 "radiologist.net",
                                                 "ravemail.com",
                                                 "realemail.net",
                                                 "reallyfast.biz",
                                                 "reallyfast.info",
                                                 "realtyagent.com",
                                                 "reborn.com",
                                                 "rediff.com",
                                                 "reggaefan.com",
                                                 "registerednurses.com",
                                                 "reincarnate.com",
                                                 "religious.com",
                                                 "repairman.com",
                                                 "representative.com",
                                                 "rescueteam.com",
                                                 "rocketmail.com",
                                                 "rocketship.com",
                                                 "runbox.com",
                                                 "rushpost.com",
                                                 "safrica.com",
                                                 "saintly.com",
                                                 "salesperson.net",
                                                 "samerica.com",
                                                 "sanfranmail.com",
                                                 "scientist.com",
                                                 "scotlandmail.com",
                                                 "secretary.net",
                                                 "sent.as",
                                                 "sent.at",
                                                 "sent.com",
                                                 "seznam.cz",
                                                 "snakebite.com",
                                                 "socialworker.net",
                                                 "sociologist.com",
                                                 "solution4u.com",
                                                 "songwriter.net",
                                                 "spainmail.com",
                                                 "spamgourmet.com",
                                                 "speedpost.net",
                                                 "speedymail.org",
                                                 "ssl-mail.com",
                                                 "surgical.net",
                                                 "swedenmail.com",
                                                 "swift-mail.com",
                                                 "swissmail.com",
                                                 "teachers.org",
                                                 "tech-center.com",
                                                 "techie.com",
                                                 "technologist.com",
                                                 "telstra.com",
                                                 "telstra.com.au",
                                                 "the-fastest.net",
                                                 "the-quickest.com",
                                                 "theinternetemail.com",
                                                 "theplate.com",
                                                 "therapist.net",
                                                 "toke.com",
                                                 "toothfairy.com",
                                                 "torontomail.com",
                                                 "tpg.com.au",
                                                 "trashmail.net",
                                                 "tvstar.com",
                                                 "umpire.com",
                                                 "usa.com",
                                                 "uymail.com",
                                                 "veryfast.biz",
                                                 "veryspeedy.net",
                                                 "virginbroadband.com.au",
                                                 "warpmail.net",
                                                 "webname.com",
                                                 "westnet.com.au",
                                                 "windowslive.com",
                                                 "worker.com",
                                                 "workmail.com",
                                                 "writeme.com",
                                                 "xsmail.com",
                                                 "xtra.co.nz",
                                                 "y7mail.com",
                                                 "yahoo.ae",
                                                 "yahoo.at",
                                                 "yahoo.be",
                                                 "yahoo.ca",
                                                 "yahoo.ch",
                                                 "yahoo.cn",
                                                 "yahoo.co.id",
                                                 "yahoo.co.il",
                                                 "yahoo.co.in",
                                                 "yahoo.co.jp",
                                                 "yahoo.co.kr",
                                                 "yahoo.co.nz",
                                                 "yahoo.co.th",
                                                 "yahoo.co.uk",
                                                 "yahoo.co.za",
                                                 "yahoo.com",
                                                 "yahoo.com.ar",
                                                 "yahoo.com.au",
                                                 "yahoo.com.br",
                                                 "yahoo.com.cn",
                                                 "yahoo.com.co",
                                                 "yahoo.com.hk",
                                                 "yahoo.com.mx",
                                                 "yahoo.com.my",
                                                 "yahoo.com.ph",
                                                 "yahoo.com.sg",
                                                 "yahoo.com.tr",
                                                 "yahoo.com.tw",
                                                 "yahoo.com.vn",
                                                 "yahoo.cz",
                                                 "yahoo.de",
                                                 "yahoo.dk",
                                                 "yahoo.es",
                                                 "yahoo.fi",
                                                 "yahoo.fr",
                                                 "yahoo.gr",
                                                 "yahoo.hu",
                                                 "yahoo.ie",
                                                 "yahoo.in",
                                                 "yahoo.it",
                                                 "yahoo.nl",
                                                 "yahoo.no",
                                                 "yahoo.pl",
                                                 "yahoo.pt",
                                                 "yahoo.ro",
                                                 "yahoo.ru",
                                                 "yahoo.se",
                                                 "yandex.ru",
                                                 "yepmail.net",
                                                 "ymail.com",
                                                 "your-mail.com",
                                                 "zoho.com"});

  return kKnownConsumerDomains.contains(email_domain);
}

#if BUILDFLAG(IS_ANDROID)

std::string GetBrowserManagerName(Profile* profile) {
  DCHECK(profile);

  // @TODO(https://crbug.com/1227786): There are some use-cases where the
  // expected behavior of chrome://management is to show more than one domain.
  absl::optional<std::string> manager = GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = GetDeviceManagerIdentity();
  }
  return manager.value_or(std::string());
}

// static
jboolean JNI_ManagedBrowserUtils_IsBrowserManaged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return IsBrowserManaged(ProfileAndroid::FromProfileAndroid(profile));
}

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_ManagedBrowserUtils_GetBrowserManagerName(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& profile) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetBrowserManagerName(ProfileAndroid::FromProfileAndroid(profile)));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_util
}  // namespace chrome
