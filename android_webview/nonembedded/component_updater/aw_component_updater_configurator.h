// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATER_CONFIGURATOR_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATER_CONFIGURATOR_H_

#include "base/memory/scoped_refptr.h"
#include "components/update_client/configurator.h"

class PrefService;

namespace base {
class CommandLine;
}

namespace android_webview {

scoped_refptr<update_client::Configurator> MakeAwComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    PrefService* pref_service);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATER_CONFIGURATOR_H_
