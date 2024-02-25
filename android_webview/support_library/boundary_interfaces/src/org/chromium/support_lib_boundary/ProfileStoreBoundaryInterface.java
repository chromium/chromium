// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.lang.reflect.InvocationHandler;
import java.util.List;

public interface ProfileStoreBoundaryInterface {
    /* ProfileBoundaryInterface */ InvocationHandler getOrCreateProfile(String name);

    /* ProfileBoundaryInterface */ InvocationHandler getProfile(String name);

    List<String> getAllProfileNames();

    boolean deleteProfile(String name);
}
