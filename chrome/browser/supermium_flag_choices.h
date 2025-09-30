// Copyright (c) 2025 Legacy Software Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPERMIUM_FLAG_CHOICES_H_
#define SUPERMIUM_FLAG_CHOICES_H_

#include "base/features.h"

const FeatureEntry::Choice kTabHoverCards[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"None",
     "tab-hover-cards",
     "none"},
    {"Tooltip",
     "tab-hover-cards",
     "tooltip"},
};

const FeatureEntry::Choice kBookmarkBarNewTab[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Never",
     "bookmark-bar-ntp",
     "never"},
};
const FeatureEntry::Choice kOmniboxAutocompleteFiltering[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Search suggestions only",
     "omnibox-autocomplete-filtering",
     "search"},
    {"Search suggestions and bookmarks",
     "omnibox-autocomplete-filtering",
     "search-bookmarks"},
    {"Search suggestions and internal chrome pages",
     "omnibox-autocomplete-filtering",
     "search-chrome"},
    {"Search suggestions, bookmarks, and internal chrome pages",
     "omnibox-autocomplete-filtering",
     "search-bookmarks-chrome"},
};
const FeatureEntry::Choice kExtensionHandlingChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Download as regular file",
     "extension-mime-request-handling",
     "download-as-regular-file"},
    {"Always prompt for install",
     "extension-mime-request-handling",
     "always-prompt-for-install"},
};
const FeatureEntry::Choice kMaxConnectionsPerHostChoices[] = {
    {"6", "", ""},
    {"15", "max-connections-per-host", "15"},
};
const FeatureEntry::Choice kShowAvatarButtonChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Always",
     "show-avatar-button",
     "always"},
    {"Incognito and Guest",
     "show-avatar-button",
     "incognito-and-guest"},
    {"Never",
     "show-avatar-button",
     "never"}
};
const FeatureEntry::Choice kCloseWindowWithLastTab[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Never",
     "close-window-with-last-tab",
     "never"},
};
const FeatureEntry::Choice kSupermiumTabOptions[] = {
    {"v109 Tabs", "", ""},
    {"v60 Tabs",
     "supermium-tab-options",
     "v60"},
    {"CR23 Tabs",
     "supermium-tab-options",
     "cr23"},
    {"Rectangular Tabs",
     "supermium-tab-options",
     "rectangular"},
};
const FeatureEntry::Choice kOpenBookmarkOptions[] = {
    {"Current Tab", "", ""},
    {"New Foreground Tab",
     "open-bookmark-option",
     "foreground"},
    {"New Background Tab",
     "open-bookmark-option",
     "background"},
};
const FeatureEntry::Choice kAutoplayPolicy[] = {
    {"Autoplay Enabled", "", ""},
    {"Autoplay Disabled",
     "autoplay-policy",
     "document-user-activation-required"},
};
#endif  // SUPERMIUM_FLAG_CHOICES_H_
