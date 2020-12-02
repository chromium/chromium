// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

/**
 * Provides access to sign-in related services that are profile-keyed on the native side. Java
 * equivalent of AccountTrackerServiceFactory and similar classes.
 *
 * TODO(https://crbug.com/1152718): Remove this class after cleaning up downstream dependencies in
 * //clank
 */
@Deprecated
public class IdentityServicesProvider
        extends org.chromium.chrome.browser.signin.services.IdentityServicesProvider {}
