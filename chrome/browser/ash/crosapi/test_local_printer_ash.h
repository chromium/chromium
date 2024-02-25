// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_LOCAL_PRINTER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_LOCAL_PRINTER_ASH_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chromeos/printing/ppd_provider.h"

class Profile;

// TestLocalPrinterAsh is used to test the LocalPrinterAsh class
// with a testing profile and fake ppd provider.
class TestLocalPrinterAsh : public crosapi::LocalPrinterAsh {
 public:
  TestLocalPrinterAsh(Profile* profile,
                      scoped_refptr<chromeos::PpdProvider> ppd_provider);
  TestLocalPrinterAsh(const TestLocalPrinterAsh&) = delete;
  TestLocalPrinterAsh& operator=(const TestLocalPrinterAsh&) = delete;
  ~TestLocalPrinterAsh() override;

 private:
  // crosapi::LocalPrinterAsh:
  Profile* GetProfile() override;
  scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(
      Profile* profile) override;

  const raw_ptr<Profile> profile_;
  const scoped_refptr<chromeos::PpdProvider> ppd_provider_;
};

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_LOCAL_PRINTER_ASH_H_
