// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PPD_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_PPD_PROVIDER_FACTORY_H_

#include "base/memory/ref_counted.h"

class Profile;

namespace chromeos {
class PpdProvider;
}  // namespace chromeos

namespace ash {

scoped_refptr<chromeos::PpdProvider> CreatePpdProvider(Profile* profile);

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::CreatePpdProvider;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_PRINTING_PPD_PROVIDER_FACTORY_H_
