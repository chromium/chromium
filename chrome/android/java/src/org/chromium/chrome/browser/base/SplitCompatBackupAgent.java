// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.app.backup.BackupAgent;
import android.app.backup.BackupDataInput;
import android.app.backup.BackupDataOutput;
import android.content.Context;
import android.os.ParcelFileDescriptor;

import org.chromium.base.BundleUtils;

import java.io.IOException;

/**
 * BackupAgent base class which will call through to the given {@link Impl}. This class must be
 * present in the base module, while the Impl can be in the chrome module.
 */
public class SplitCompatBackupAgent extends BackupAgent {
    private String mBackupAgentClassName;
    private Impl mImpl;

    public SplitCompatBackupAgent(String backupAgentClassName) {
        mBackupAgentClassName = backupAgentClassName;
    }

    @Override
    protected void attachBaseContext(Context context) {
        context = SplitCompatApplication.createChromeContext(context);
        mImpl = (Impl) BundleUtils.newInstance(context, mBackupAgentClassName);
        mImpl.setBackupAgent(this);
        super.attachBaseContext(context);
    }

    @Override
    public void onBackup(
            ParcelFileDescriptor oldState, BackupDataOutput data, ParcelFileDescriptor newState)
            throws IOException {
        mImpl.onBackup(oldState, data, newState);
    }

    @Override
    public void onRestore(BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
            throws IOException {
        mImpl.onRestore(data, appVersionCode, newState);
    }

    /**
     * Holds the implementation of backup agent logic. Will be called by {@link
     * SplitCompatBackupAgent}.
     */
    public abstract static class Impl {
        private SplitCompatBackupAgent mBackupAgent;

        protected final void setBackupAgent(SplitCompatBackupAgent backupAgent) {
            mBackupAgent = backupAgent;
        }

        protected final BackupAgent getBackupAgent() {
            return mBackupAgent;
        }

        public abstract void onBackup(
                ParcelFileDescriptor oldState, BackupDataOutput data, ParcelFileDescriptor newState)
                throws IOException;

        public abstract void onRestore(
                BackupDataInput data, int appVersionCode, ParcelFileDescriptor newState)
                throws IOException;
    }
}
