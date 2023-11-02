// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dependency_injection;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Scope;

/**
 * Scope of components living as long as activities associated with them live.
 * Can be used for all activities, since there is no way for components of different activities (as
 * well as different instances of the same activity) to communicate with each other via Dagger.
 */
@Scope
@Retention(RetentionPolicy.SOURCE)
public @interface ActivityScope {}
