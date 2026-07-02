// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.InvalidProtocolBufferException;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.ShareActivatedEntryPoint;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.proto.SendTabToSelfPersistedTabData.SendTabToSelfPersistedTabDataProto;

import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

/** PersistedTabData attached to a Tab to store the sender device name for Send Tab To Self. */
@NullMarked
@JNINamespace("send_tab_to_self")
public class SendTabToSelfTabCardLabelData extends PersistedTabData {
    // The expiration time for the data, same as the STTS entry expiration time.
    private static final long EXPIRATION_MS = TimeUnit.DAYS.toMillis(10); // 10 days
    // The expiration time for the visual label, which is shorter than the data expiration time.
    private static final long LABEL_EXPIRATION_MS = TimeUnit.DAYS.toMillis(5); // 5 days
    private static final String TAG = "SendTabToSelfTabCardLabelData";
    private static final Class<SendTabToSelfTabCardLabelData> USER_DATA_KEY =
            SendTabToSelfTabCardLabelData.class;

    private String mGuid;
    private String mSenderDeviceName;
    private long mAdditionTimestampMs;

    @SuppressWarnings("HidingField")
    private final SettableMonotonicObservableSupplier<Boolean> mIsPersistenceEnabledSupplier =
            ObservableSuppliers.createMonotonic();

    private boolean mObserverRegistered;

    /**
     * Observer that removes the SendTabToSelfTabCardLabelData from the UserDataHost when the tab is
     * interacted with by the user.
     */
    private final EmptyTabObserver mObserver =
            new EmptyTabObserver() {
                @Override
                public void onShown(Tab tab, @TabSelectionType int type) {
                    if (tab != mTab) return;
                    SendTabToSelfTabCardLabelDataJni.get()
                            .markEntryActivated(
                                    tab.getProfile(), mGuid, ShareActivatedEntryPoint.TAB_STRIP);
                    removeAndDestroy();
                }

                @Override
                public void onDestroyed(Tab tab) {
                    if (tab != mTab) return;
                    // Only mark the entry as activated if the tab is closed, and not upon browser
                    // shutdown.
                    if (tab.isClosing()) {
                        SendTabToSelfTabCardLabelDataJni.get()
                                .markEntryActivated(
                                        tab.getProfile(),
                                        mGuid,
                                        ShareActivatedEntryPoint
                                                .TAB_OR_BROWSER_CLOSED_WITHOUT_ACTIVATION);
                        removeAndDestroy();
                    }
                }
            };

    /** Removes the data from the host and destroys the instance. */
    private void removeAndDestroy() {
        UserDataHost host = mTab.getUserDataHost();
        // A race-condition is possible due to the asynchronous nature of PersistedTabData.from().
        if (host != null && host.getUserData(SendTabToSelfTabCardLabelData.class) == this) {
            host.removeUserData(SendTabToSelfTabCardLabelData.class);
            delete();
            destroy();
        }
    }

    /** Returns true if this is a negative cache instance (empty data), false otherwise. */
    public boolean isNegativeCache() {
        return mSenderDeviceName.isEmpty();
    }

    /** Safely registers the observer on the UI thread if the data is valid. */
    private void registerTabObserverIfNotNegativeCache() {
        org.chromium.base.ThreadUtils.assertOnUiThread();
        if (!mObserverRegistered && !isNegativeCache()) {
            mTab.addObserver(mObserver);
            mObserverRegistered = true;
        }
    }

    private void updateIsPersistenceEnabledSupplier() {
        mIsPersistenceEnabledSupplier.set(
                !mTab.isIncognito()
                        && !mTab.isDestroyed()
                        // Avoid saving the negative cache instances.
                        && !isNegativeCache());
    }

    /**
     * Returns whether the label data has expired. Note: Upon expiration, the metric logging takes
     * place upon garbage collection in SendTabToSelfBridge. So it does not happen here.
     *
     * @return True if the data has exceeded the 5-day expiration window, false otherwise.
     */
    private boolean isExpired() {
        // Avoid marking the negative cache as expired.
        if (isNegativeCache()) return false;
        return System.currentTimeMillis() - mAdditionTimestampMs > EXPIRATION_MS;
    }

    /**
     * Returns whether the visual label should be shown.
     *
     * @return True if the data is not expired and is within the 5-day label window, false
     *     otherwise.
     */
    public boolean shouldShowLabel() {
        if (isNegativeCache()) return false;
        return System.currentTimeMillis() - mAdditionTimestampMs < LABEL_EXPIRATION_MS;
    }

    /**
     * Asynchronously gets the SendTabToSelfTabCardLabelData associated with the given tab.
     *
     * @param tab The Tab to get the tab data for.
     * @param callback The Callback to be invoked when the SendTabToSelfTabCardLabelData is ready.
     */
    public static void from(Tab tab, Callback<@Nullable SendTabToSelfTabCardLabelData> callback) {
        PersistedTabData.from(
                tab,
                () ->
                        new SendTabToSelfTabCardLabelData(
                                tab,
                                /* guid= */ "",
                                /* senderDeviceName= */ "",
                                /* additionTimestampMs= */ 0),
                USER_DATA_KEY,
                (data) -> {
                    if (data != null && data.isExpired()) {
                        data.removeAndDestroy();
                        callback.onResult(null);
                        return;
                    }
                    callback.onResult(data);
                });
    }

    /**
     * Returns the valid SendTabToSelfTabCardLabelData for the tab, removing it if expired. Note:
     * This returns the in-memory instance if already loaded/attached.
     *
     * @param tab The Tab from which to retrieve the label data.
     * @return The valid SendTabToSelfTabCardLabelData object, or null if absent or expired.
     */
    public static @Nullable SendTabToSelfTabCardLabelData get(Tab tab) {
        if (tab == null || tab.getUserDataHost() == null) return null;
        SendTabToSelfTabCardLabelData data =
                tab.getUserDataHost().getUserData(SendTabToSelfTabCardLabelData.class);
        if (data == null) return null;

        if (data.isExpired()) {
            data.removeAndDestroy();
            return null;
        }
        return data;
    }

    /**
     * Constructs a new SendTabToSelfTabCardLabelData object and attaches it as a TabObserver.
     *
     * @param tab The Tab to which this label data is attached.
     * @param senderDeviceName The name of the device that sent the tab.
     * @param additionTimestampMs The timestamp in milliseconds when the tab was added in the
     *     background.
     */
    public SendTabToSelfTabCardLabelData(
            Tab tab, String guid, String senderDeviceName, long additionTimestampMs) {
        super(
                tab,
                PersistedTabDataConfiguration.get(
                                SendTabToSelfTabCardLabelData.class, tab.isIncognito())
                        .getStorage(),
                PersistedTabDataConfiguration.get(
                                SendTabToSelfTabCardLabelData.class, tab.isIncognito())
                        .getId());
        mGuid = guid;
        mSenderDeviceName = senderDeviceName;
        mAdditionTimestampMs = additionTimestampMs;

        registerTabObserverIfNotNegativeCache();

        registerIsTabSaveEnabledSupplier(mIsPersistenceEnabledSupplier);
        updateIsPersistenceEnabledSupplier();
    }

    /** Destroys the UserData object and removes the observer from the tab. */
    @Override
    public void destroy() {
        mTab.removeObserver(mObserver);
        super.destroy();
    }

    /**
     * Returns the localized label text to display on the tab card.
     *
     * @param context The Context used to retrieve localized string resources.
     * @return The localized label text indicating the sender device.
     */
    public String getLabelText(Context context) {
        return context.getString(
                R.string.send_tab_to_self_message_banner_subtitle, mSenderDeviceName);
    }

    /**
     * Sets the addition timestamp for testing purposes.
     *
     * @param additionTimestampMs The timestamp to set.
     */
    @VisibleForTesting
    public void setAdditionTimestampMsForTesting(long additionTimestampMs) {
        mAdditionTimestampMs = additionTimestampMs;
    }

    @VisibleForTesting
    public String getGuidForTesting() {
        return mGuid;
    }

    @VisibleForTesting
    public String getSenderDeviceNameForTesting() {
        return mSenderDeviceName;
    }

    @VisibleForTesting
    public long getAdditionTimestampMsForTesting() {
        return mAdditionTimestampMs;
    }

    // PersistedTabData implementation.

    @Override
    Serializer<ByteBuffer> getSerializer() {
        return () ->
                SendTabToSelfPersistedTabDataProto.newBuilder()
                        .setGuid(mGuid)
                        .setSenderDeviceName(mSenderDeviceName)
                        .setAdditionTimestampMs(mAdditionTimestampMs)
                        .build()
                        .toByteString()
                        .asReadOnlyByteBuffer();
    }

    @Override
    boolean deserialize(@Nullable ByteBuffer bytes) {
        if (bytes == null || !bytes.hasRemaining()) return false;

        try {
            SendTabToSelfPersistedTabDataProto proto =
                    SendTabToSelfPersistedTabDataProto.parseFrom(bytes);
            mGuid = proto.getGuid();
            mSenderDeviceName = proto.getSenderDeviceName();
            mAdditionTimestampMs = proto.getAdditionTimestampMs();
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        updateIsPersistenceEnabledSupplier();
                        registerTabObserverIfNotNegativeCache();
                    });
        } catch (InvalidProtocolBufferException e) {
            Log.i(TAG, "deserialize failed: \n" + e.toString());
            return false;
        }

        return true;
    }

    @Override
    public String getUmaTag() {
        return TAG;
    }

    @NativeMethods
    public interface Natives {
        void markEntryActivated(
                @JniType("Profile*") Profile profile,
                String guid,
                @ShareActivatedEntryPoint int entryPoint);
    }
}
