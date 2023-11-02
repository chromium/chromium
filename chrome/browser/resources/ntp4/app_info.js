// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{app_launch_ordinal: string,
 *            description: string,
 *            detailsUrl: string,
 *            direction: string,
 *            enabled: boolean,
 *            full_name: string,
 *            full_name_direction: string,
 *            homepageUrl: string,
 *            icon_big: string,
 *            icon_big_exists: boolean,
 *            icon_small: string,
 *            icon_small_exists: boolean,
 *            id: string,
 *            is_component: boolean,
 *            is_deprecated_app: boolean,
 *            is_webstore: boolean,
 *            isLocallyInstalled: boolean,
 *            hideDisplayMode: boolean,
 *            kioskEnabled: boolean,
 *            kioskMode: boolean,
 *            kioskOnly: boolean,
 *            launch_container: number,
 *            launch_type: number,
 *            mayChangeLaunchType: boolean,
 *            mayShowRunOnOsLoginMode: boolean,
 *            mayToggleRunOnOsLoginMode: boolean,
 *            mayCreateShortcuts: boolean,
 *            mayDisable: boolean,
 *            name: string,
 *            offlineEnabled: boolean,
 *            optionsUrl: string,
 *            packagedApp: boolean,
 *            page_index: number,
 *            runOnOsLoginMode: string,
 *            title: string,
 *            url: string,
 *            version: string}}
 * @see chrome/browser/ui/webui/ntp/app_launcher_handler.cc
 */
export let AppInfo;
