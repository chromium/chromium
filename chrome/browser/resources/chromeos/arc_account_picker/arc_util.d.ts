// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface AccountAdditionOptions {
  isAvailableInArc: boolean;
  showArcAvailabilityPicker: boolean;
}

export function getAccountAdditionOptionsFromJSON(json: string|null):
    AccountAdditionOptions|null;
