// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/server_certificate_database/server_certificate_database_service.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/task/bind_post_task.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "net/cert/nss_cert_database.h"
#endif

namespace net {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
void GotNSSCertDatabaseOnIOThread(
    base::OnceCallback<void(crypto::ScopedPK11Slot)> callback,
    NSSCertDatabase* db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(db->GetPublicSlot());
}

void NssSlotGetter(NssCertDatabaseGetter nss_cert_db_getter,
                   base::OnceCallback<void(crypto::ScopedPK11Slot)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&GotNSSCertDatabaseOnIOThread, std::move(callback)));

  net::NSSCertDatabase* cert_db =
      std::move(nss_cert_db_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db) {
    std::move(split_callback.second).Run(cert_db);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

ServerCertificateDatabaseService*
ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<ServerCertificateDatabaseService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

ServerCertificateDatabaseServiceFactory*
ServerCertificateDatabaseServiceFactory::GetInstance() {
  static base::NoDestructor<ServerCertificateDatabaseServiceFactory> instance;
  return instance.get();
}

ServerCertificateDatabaseServiceFactory::
    ServerCertificateDatabaseServiceFactory()
    : ProfileKeyedServiceFactory(
          "ServerCertificateDatabaseService",
          base::FeatureList::IsEnabled(
              ::features::kEnableCertManagementUIV2Write)
              ?
              // Use the same service for incognito profiles.
              ProfileSelections::Builder()
                  .WithRegular(ProfileSelection::kRedirectedToOriginal)
                  // For Guest the need for these these are based off of what
                  // ProfileNetworkContextService does.
                  .WithGuest(ProfileSelection::kRedirectedToOriginal)
                  // Not needed for Ash internals as it's not a real user
                  // profile and so there isn't a user to use the database.
                  // This also matches the practical behavior of
                  // NssServiceFactory which will end up crashing the browser
                  // if attempted to use on an AshInternals profile.
                  .WithAshInternals(ProfileSelection::kNone)
                  .Build()
              : ProfileSelections::BuildNoProfilesSelected()

      ) {
#if BUILDFLAG(IS_CHROMEOS)
  DependsOn(NssServiceFactory::GetInstance());
#endif
}

ServerCertificateDatabaseServiceFactory::
    ~ServerCertificateDatabaseServiceFactory() = default;

std::unique_ptr<KeyedService>
ServerCertificateDatabaseServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ServerCertificateDatabaseService>(
      profile->GetPath(), profile->GetPrefs(),
      base::BindPostTask(
          content::GetIOThreadTaskRunner({}),
          base::BindOnce(&NssSlotGetter,
                         NssServiceFactory::GetForContext(profile)
                             ->CreateNSSCertDatabaseGetterForIOThread())));
#else
  return std::make_unique<ServerCertificateDatabaseService>(profile->GetPath());
#endif
}

}  // namespace net
