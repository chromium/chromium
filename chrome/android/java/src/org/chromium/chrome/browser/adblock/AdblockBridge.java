/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-present eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.chromium.chrome.browser.adblock;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * AdblockBridge is a singleton which provides access to some native adblocking methods.
 */
public final class AdblockBridge {

    private AdblockBridge() {
    }

    private static AdblockBridge sInstance;
    
    /**
     * @return The singleton object.
     */
    public static AdblockBridge getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new AdblockBridge();
        return sInstance;
    }

    /**
     * @return Whether the singleton have been initialized.
     */
    public static boolean isInitialized() {
        return sInstance != null;
    }

    public void setFilterEngineNativePtr(long ptr) {
        nativeSetFilterEngineNativePtr(ptr);
    }

    public long getIsolateProviderNativePtr() {
        return nativeGetIsolateProviderNativePtr();
    }

    private native void nativeSetFilterEngineNativePtr(long ptr);
    private native long nativeGetIsolateProviderNativePtr();
}
