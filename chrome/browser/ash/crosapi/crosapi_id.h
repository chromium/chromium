// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ID_H_
#define CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ID_H_

#include "base/types/id_type.h"

namespace crosapi {
namespace internal {
struct CrosapiIdTag {};
}  // namespace internal

// CrosapiId is an id created on a new Crosapi connection creation.
// This will be useful to identify what bindings/remote of sub crosapi
// interfaces are related each other.
using CrosapiId = base::IdTypeU32<internal::CrosapiIdTag>;

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CROSAPI_ID_H_
