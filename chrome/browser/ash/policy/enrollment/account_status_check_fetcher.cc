// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/account_status_check_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// List of consumer-only domains from the server side logic. See
// `KNOWN_INVALID_DOMAINS` from GetAgencySignupStateProducerModule.java.
const char* const kKnownConsumerDomains[] = {"123mail.org",
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
                                             "zoho.com"};

AccountStatusCheckFetcher::AccountStatus ParseStatus(
    const em::CheckUserAccountResponse& response,
    const std::string& email) {
  if (!response.has_user_account_type()) {
    return AccountStatusCheckFetcher::AccountStatus::kUnknown;
  }
  if (response.user_account_type() ==
      em::CheckUserAccountResponse::UNKNOWN_USER_ACCOUNT_TYPE) {
    return AccountStatusCheckFetcher::AccountStatus::kUnknown;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::CONSUMER) {
    const std::string domain = gaia::ExtractDomainName(email);
    if (base::Contains(kKnownConsumerDomains, domain)) {
      return AccountStatusCheckFetcher::AccountStatus::
          kConsumerWithConsumerDomain;
    }
    return AccountStatusCheckFetcher::AccountStatus::
        kConsumerWithBusinessDomain;
  }
  if (response.user_account_type() == em::CheckUserAccountResponse::DASHER) {
    return AccountStatusCheckFetcher::AccountStatus::kDasher;
  }

  if (response.user_account_type() == em::CheckUserAccountResponse::NOT_EXIST) {
    if (!response.has_domain_verified()) {
      return AccountStatusCheckFetcher::AccountStatus::kUnknown;
    }
    if (response.domain_verified()) {
      return AccountStatusCheckFetcher::AccountStatus::
          kOrganisationalAccountVerified;
    }
    return AccountStatusCheckFetcher::AccountStatus::
        kOrganisationalAccountUnverified;
  }
  return AccountStatusCheckFetcher::AccountStatus::kUnknown;
}

void RecordAccountStatusCheckResult(
    AccountStatusCheckFetcher::AccountStatus value) {
  base::UmaHistogramEnumeration("Enterprise.AccountStatusCheckResult", value);
}

}  // namespace

AccountStatusCheckFetcher::AccountStatusCheckFetcher(
    const std::string& canonicalized_email)
    : AccountStatusCheckFetcher(
          canonicalized_email,
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->device_management_service(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory()) {}

AccountStatusCheckFetcher::AccountStatusCheckFetcher(
    const std::string& canonicalized_email,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : email_(canonicalized_email),
      service_(service),
      url_loader_factory_(url_loader_factory),
      random_device_id_(base::GenerateGUID()) {}

AccountStatusCheckFetcher::~AccountStatusCheckFetcher() = default;

void AccountStatusCheckFetcher::Fetch(FetchCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = std::move(callback);
  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          service_,
          DeviceManagementService::JobConfiguration::TYPE_CHECK_USER_ACCOUNT,
          random_device_id_, /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/absl::nullopt, url_loader_factory_,
          base::BindOnce(
              &AccountStatusCheckFetcher::OnAccountStatusCheckReceived,
              weak_ptr_factory_.GetWeakPtr()));

  em::CheckUserAccountRequest* request =
      config->request()->mutable_check_user_account_request();
  request->set_user_email(email_);
  fetch_request_job_ = service_->CreateJob(std::move(config));
}

void AccountStatusCheckFetcher::OnAccountStatusCheckReceived(
    DMServerJobResult result) {
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Account check response received. DM Status: "
               << result.dm_status;

  fetch_request_job_.reset();
  std::string user_id;
  bool fetch_succeeded = false;
  switch (result.dm_status) {
    case DM_STATUS_SUCCESS: {
      if (!result.response.has_check_user_account_response()) {
        LOG(WARNING) << "Invalid Account check response.";
        break;
      }

      // Fetch has succeeded.
      fetch_succeeded = true;
      result_ =
          ParseStatus(result.response.check_user_account_response(), email_);
      RecordAccountStatusCheckResult(result_);
      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Account check failed. DM Status: " << result.dm_status;
      break;
    }
  }
  std::move(callback_).Run(fetch_succeeded, result_);
}

}  // namespace policy
