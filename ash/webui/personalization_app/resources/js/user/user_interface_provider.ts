// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview a singleton getter for the user mojom interface used in
 * the Personalization SWA. Also contains utility function for mocking out the
 * implementation for testing.
 */

import 'chrome://resources/mojo/mojo/public/js/bindings.js';

import {UserProvider, UserProviderInterface} from '../../personalization_app.mojom-webui.js';

let userProvider: UserProviderInterface|null = null;

export function setUserProviderForTesting(testProvider: UserProviderInterface):
    void {
  userProvider = testProvider;
}

/** Returns a singleton for the UserProvider mojom interface. */
export function getUserProvider(): UserProviderInterface {
  if (!userProvider) {
    userProvider = UserProvider.getRemote();
  }
  return userProvider;
}
