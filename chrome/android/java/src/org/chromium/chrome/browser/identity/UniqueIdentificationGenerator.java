// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

/**
 * Interface used for uniquely identifying an installation of Chrome. To get an instance you should
 * use {@link UniqueIdentificationGeneratorFactory}.
 */
public interface UniqueIdentificationGenerator
        extends org.chromium.chrome.browser.uid.UniqueIdentificationGenerator {}
