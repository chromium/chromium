// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_REMOVER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_REMOVER_H_

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// TODO(rsleevi): Remove in M86 or later.
void DeleteLegacySTHSet(const base::FilePath& user_data_dir);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_STH_SET_COMPONENT_REMOVER_H_
