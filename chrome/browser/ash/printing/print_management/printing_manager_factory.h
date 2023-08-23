// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/printing/print_management/printing_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class WebUI;
class WebUIController;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class GURL;
class Profile;

namespace ash {
namespace printing {
namespace print_management {

class PrintingManager;

// Factory for PrintingManager.
class PrintingManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PrintingManager* GetForProfile(Profile* profile);
  static PrintingManagerFactory* GetInstance();
  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* profile);
  static void MaybeBindPrintManagementForWebUI(
      Profile* profile,
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
          receiver);
  static std::unique_ptr<content::WebUIController>
  CreatePrintManagementUIController(content::WebUI* web_ui, const GURL& url);

 private:
  friend base::NoDestructor<PrintingManagerFactory>;

  PrintingManagerFactory();
  ~PrintingManagerFactory() override;

  PrintingManagerFactory(const PrintingManagerFactory&) = delete;
  PrintingManagerFactory& operator=(const PrintingManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace print_management
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
