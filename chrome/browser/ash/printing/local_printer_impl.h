// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/printing/local_printer.h"

class ApplicationLocaleStorage;
class Profile;

namespace chromeos {
class PpdProvider;
}

namespace ash {

class LocalPrinterImpl : public LocalPrinter {
 public:
  // `application_locale_storage` must be non-null and outlive this.
  explicit LocalPrinterImpl(
      const ApplicationLocaleStorage* application_locale_storage);
  LocalPrinterImpl(const LocalPrinterImpl&) = delete;
  LocalPrinterImpl& operator=(const LocalPrinterImpl&) = delete;
  ~LocalPrinterImpl() override;

  // LocalPrinter override:
  // Guest users are not supported for all functions.
  void GetPrinters(const AccountId& accountId,
                   GetPrintersCallback callback) override;
  std::optional<chromeos::Printer> GetPrinter(
      const AccountId& accountId,
      const std::string& printer_id) override;
  void GetCapability(const AccountId& accountId,
                     const std::string& printer_id,
                     GetCapabilityCallback callback) override;
  void GetStatus(const AccountId& accountId,
                 const std::string& printer_id,
                 GetStatusCallback callback) override;
  void GetEulaUrl(const AccountId& accountId,
                  const std::string& printer_id,
                  GetEulaUrlCallback callback) override;
  void GetOAuthAccessToken(const AccountId& accountId,
                           const std::string& printer_id,
                           GetOAuthAccessTokenCallback callback) override;

 protected:
  virtual scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(
      Profile* profile);

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_LOCAL_PRINTER_IMPL_H_
