// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.graphics.Color;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tab.WebContentsStateBridge;
import org.chromium.chrome.browser.tab.proto.CriticalPersistedTabData.CriticalPersistedTabDataProto;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.Locale;

/**
 * Data which is core to the app and must be retrieved as quickly as possible on startup.
 */
public class CriticalPersistedTabData extends PersistedTabData {
    private static final String TAG = "CriticalPTD";
    private static final Class<CriticalPersistedTabData> USER_DATA_KEY =
            CriticalPersistedTabData.class;

    private static final int UNSPECIFIED_THEME_COLOR = Color.TRANSPARENT;
    public static final long INVALID_TIMESTAMP = -1;

    /**
     * Title of the ContentViews webpage.
     */
    private String mTitle;

    /**
     * URL of the page currently loading. Used as a fall-back in case tab restore fails.
     */
    private GURL mUrl;
    private int mParentId;
    private int mRootId;
    private long mTimestampMillis;
    /**
     * Navigation state of the WebContents as returned by nativeGetContentsStateAsByteBuffer(),
     * stored to be inflated on demand using unfreezeContents(). If this is not null, there is no
     * WebContents around. Upon tab switch WebContents will be unfrozen and the variable will be set
     * to null.
     */
    private WebContentsState mWebContentsState;
    private int mContentStateVersion;
    private String mOpenerAppId;
    private int mThemeColor;
    /**
     * Saves how this tab was initially launched so that we can record metrics on how the
     * tab was created. This is different than {@link Tab#getLaunchType()}, since {@link
     * Tab#getLaunchType()} will be overridden to "FROM_RESTORE" during tab restoration.
     */
    private @Nullable @TabLaunchType Integer mTabLaunchTypeAtCreation;

    private ObserverList<CriticalPersistedTabDataObserver> mObservers =
            new ObserverList<CriticalPersistedTabDataObserver>();

    private boolean mIsStorageRetrievalEnabled;

    private CriticalPersistedTabData(Tab tab) {
        super(tab,
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, tab.isIncognito())
                        .storage,
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, tab.isIncognito())
                        .id);
    }

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param parentId parent identiifer for the {@link Tab}
     * @param rootId root identifier for the {@link Tab}
     * @param timestampMillis creation timestamp for the {@link Tab}
     * @param contentStateBytes content state bytes for the {@link Tab}
     * @param contentStateVersion content state version for the {@link Tab}
     * @param openerAppId identifier for app opener
     * @param themeColor theme color
     * @param launchTypeAtCreation launch type at creation
     * @param persistedTabDataStorage storage for {@link PersistedTabData}
     * @param persistedTabDataId identifier for {@link PersistedTabData} in storage
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    CriticalPersistedTabData(Tab tab, String url, String title, int parentId, int rootId,
            long timestampMillis, WebContentsState webContentsState, int contentStateVersion,
            String openerAppId, int themeColor,
            @Nullable @TabLaunchType Integer launchTypeAtCreation) {
        this(tab);
        mUrl = url == null || url.isEmpty() ? GURL.emptyGURL() : new GURL(url);
        mTitle = title;
        mParentId = parentId;
        mRootId = rootId;
        mTimestampMillis = timestampMillis;
        mWebContentsState = webContentsState;
        mContentStateVersion = contentStateVersion;
        mOpenerAppId = openerAppId;
        mThemeColor = themeColor;
        mTabLaunchTypeAtCreation = launchTypeAtCreation;
    }

    /**
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for
     * @param data serialized {@link CriticalPersistedTabData}
     * @param storage {@link PersistedTabDataStorage} for {@link PersistedTabData}
     * @param persistedTabDataId unique identifier for {@link PersistedTabData} in
     * storage
     * This constructor is public because that is needed for the reflection
     * used in PersistedTabData.java
     */
    private CriticalPersistedTabData(
            Tab tab, byte[] data, PersistedTabDataStorage storage, String persistedTabDataId) {
        super(tab, data, storage, persistedTabDataId);
    }

    /**
     * TODO(crbug.com/1096142) asynchronous from can be removed
     * Acquire {@link CriticalPersistedTabData} from storage
     * @param tab {@link Tab} {@link CriticalPersistedTabData} is being stored for.
     *        At a minimum this needs to be a frozen {@link Tab} with an identifier
     *        and isIncognito values set.
     * @param callback callback to pass {@link PersistedTabData} back in
     */
    public static void from(Tab tab, Callback<CriticalPersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new CriticalPersistedTabData(tab, data, storage, id); },
                ()
                        -> {
                    if (tab.isInitialized()) {
                        return CriticalPersistedTabData.build(tab);
                    }
                    return null;
                },
                CriticalPersistedTabData.class, callback);
    }

    /**
     * Acquire {@link CriticalPersistedTabData} from a {@link Tab} or create if it doesn't exist
     * @param  corresponding {@link Tab} for which {@link CriticalPersistedTabData} is sought
     * @return acquired or created {@link CriticalPersistedTabData}
     */
    public static CriticalPersistedTabData from(Tab tab) {
        return PersistedTabData.from(
                tab, CriticalPersistedTabData.class, () -> { return build(tab); });
    }

    /**
     * Synchronously restore serialized {@link CriticalPersistedTabData}
     * @param tabId identifier for the {@link Tab}
     * @param isIncognito true if the {@link Tab} is incognito
     * @return serialized {@link CriticalPersistedTabData}
     * TODO(crbug.com/1119452) rethink CriticalPersistedTabData contract
     */
    public static byte[] restore(int tabId, boolean isIncognito) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, isIncognito);
        return config.storage.restore(tabId, config.id);
    }

    /**
     * Asynchronously restore serialized {@link CriticalPersistedTabData}
     * @param tabId identifier for the {@link Tab}
     * @param isIncognito true if the {@link Tab} is incognito
     * @param callback the serialized {@link CriticalPersistedTabData} is passed back in
     */
    public static void restore(int tabId, boolean isIncognito, Callback<byte[]> callback) {
        PersistedTabDataConfiguration config =
                PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, isIncognito);
        config.storage.restore(tabId, config.id, callback);
    }

    /**
     * @param tab {@link Tab} associated with the {@link CriticalPersistedTabData}
     * @param serialized {@link CriticalPersistedTabData} in serialized form
     * @param isCriticalPersistedTabDataEnabled true if CriticalPersistedData is enabled
     * as the storage/retrieval method
     */
    public static void build(Tab tab, byte[] serialized, boolean isStorageRetrievalEnabled) {
        CriticalPersistedTabData res = PersistedTabData.build(tab, (data, storage, id) -> {
            return new CriticalPersistedTabData(tab, data, storage, id);
        }, serialized, CriticalPersistedTabData.class);
        res.mIsStorageRetrievalEnabled = isStorageRetrievalEnabled;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static CriticalPersistedTabData build(Tab tab) {
        // CriticalPersistedTabData is initialized with default values
        CriticalPersistedTabData criticalPersistedTabData =
                new CriticalPersistedTabData(tab, "", "", Tab.INVALID_TAB_ID, tab.getId(),
                        INVALID_TIMESTAMP, null, -1, "", UNSPECIFIED_THEME_COLOR, null);
        criticalPersistedTabData.save();
        return criticalPersistedTabData;
    }

    @Override
    boolean deserialize(@Nullable byte[] bytes) {
        try (TraceEvent e = TraceEvent.scoped("CriticalPersistedTabData.Deserialize")) {
            CriticalPersistedTabDataProto criticalPersistedTabDataProto =
                    CriticalPersistedTabDataProto.parseFrom(bytes);
            mParentId = criticalPersistedTabDataProto.getParentId();
            mRootId = criticalPersistedTabDataProto.getRootId();
            mTimestampMillis = criticalPersistedTabDataProto.getTimestampMillis();
            byte[] webContentsStateBytes =
                    criticalPersistedTabDataProto.getWebContentsStateBytes().toByteArray();
            mWebContentsState =
                    new WebContentsState(ByteBuffer.allocateDirect(webContentsStateBytes.length));
            mWebContentsState.buffer().put(webContentsStateBytes);
            mWebContentsState.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
            mUrl = mWebContentsState.getVirtualUrlFromState() == null
                    ? GURL.emptyGURL()
                    : new GURL(mWebContentsState.getVirtualUrlFromState());
            mTitle = mWebContentsState.getDisplayTitleFromState();
            mContentStateVersion = criticalPersistedTabDataProto.getContentStateVersion();
            mOpenerAppId = criticalPersistedTabDataProto.getOpenerAppId();
            mThemeColor = criticalPersistedTabDataProto.getThemeColor();
            mTabLaunchTypeAtCreation =
                    getLaunchType(criticalPersistedTabDataProto.getLaunchTypeAtCreation());
            return true;
        } catch (InvalidProtocolBufferException e) {
            Log.e(TAG,
                    String.format(Locale.ENGLISH,
                            "There was a problem deserializing Tab %d. Details: %s", mTab.getId(),
                            e.getMessage()));
        }
        return false;
    }

    @Override
    public String getUmaTag() {
        return "Critical";
    }

    private static @Nullable @TabLaunchType Integer getLaunchType(
            CriticalPersistedTabDataProto.LaunchTypeAtCreation protoLaunchType) {
        switch (protoLaunchType) {
            case FROM_LINK:
                return TabLaunchType.FROM_LINK;
            case FROM_EXTERNAL_APP:
                return TabLaunchType.FROM_EXTERNAL_APP;
            case FROM_CHROME_UI:
                return TabLaunchType.FROM_CHROME_UI;
            case FROM_RESTORE:
                return TabLaunchType.FROM_RESTORE;
            case FROM_LONGPRESS_FOREGROUND:
                return TabLaunchType.FROM_LONGPRESS_FOREGROUND;
            case FROM_LONGPRESS_BACKGROUND:
                return TabLaunchType.FROM_LONGPRESS_BACKGROUND;
            case FROM_REPARENTING:
                return TabLaunchType.FROM_REPARENTING;
            case FROM_LAUNCHER_SHORTCUT:
                return TabLaunchType.FROM_LAUNCHER_SHORTCUT;
            case FROM_SPECULATIVE_BACKGROUND_CREATION:
                return TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION;
            case FROM_BROWSER_ACTIONS:
                return TabLaunchType.FROM_BROWSER_ACTIONS;
            case FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case FROM_STARTUP:
                return TabLaunchType.FROM_STARTUP;
            case FROM_START_SURFACE:
                return TabLaunchType.FROM_START_SURFACE;
            case SIZE:
                return TabLaunchType.SIZE;
            default:
                assert false : "Unexpected deserialization of LaunchAtCreationType: "
                               + protoLaunchType;
                // shouldn't happen
                return null;
        }
    }

    private static CriticalPersistedTabDataProto.LaunchTypeAtCreation getLaunchType(
            @Nullable @TabLaunchType Integer protoLaunchType) {
        if (protoLaunchType == null) {
            return CriticalPersistedTabDataProto.LaunchTypeAtCreation.UNKNOWN;
        }
        switch (protoLaunchType) {
            case TabLaunchType.FROM_LINK:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LINK;
            case TabLaunchType.FROM_EXTERNAL_APP:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_EXTERNAL_APP;
            case TabLaunchType.FROM_CHROME_UI:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_CHROME_UI;
            case TabLaunchType.FROM_RESTORE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_RESTORE;
            case TabLaunchType.FROM_LONGPRESS_FOREGROUND:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LONGPRESS_FOREGROUND;
            case TabLaunchType.FROM_LONGPRESS_BACKGROUND:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LONGPRESS_BACKGROUND;
            case TabLaunchType.FROM_REPARENTING:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_REPARENTING;
            case TabLaunchType.FROM_LAUNCHER_SHORTCUT:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_LAUNCHER_SHORTCUT;
            case TabLaunchType.FROM_SPECULATIVE_BACKGROUND_CREATION:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation
                        .FROM_SPECULATIVE_BACKGROUND_CREATION;
            case TabLaunchType.FROM_BROWSER_ACTIONS:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_BROWSER_ACTIONS;
            case TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation
                        .FROM_LAUNCH_NEW_INCOGNITO_TAB;
            case TabLaunchType.FROM_STARTUP:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_STARTUP;
            case TabLaunchType.FROM_START_SURFACE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.FROM_START_SURFACE;
            case TabLaunchType.SIZE:
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.SIZE;
            default:
                assert false : "Unexpected serialization of LaunchAtCreationType: "
                               + protoLaunchType;
                // shouldn't happen
                return CriticalPersistedTabDataProto.LaunchTypeAtCreation.UNKNOWN;
        }
    }

    private static WebContentsState getWebContentsStateFromTab(Tab tab) {
        // Native call returns null when buffer allocation needed to serialize the state failed.
        ByteBuffer buffer = getWebContentsStateAsByteBuffer(tab);
        if (buffer == null) return null;

        WebContentsState state = new WebContentsState(buffer);
        state.setVersion(WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    /** Returns an ByteBuffer representing the state of the Tab's WebContents. */
    private static ByteBuffer getWebContentsStateAsByteBuffer(Tab tab) {
        LoadUrlParams pendingLoadParams = tab.getPendingLoadParams();
        if (pendingLoadParams == null) {
            return WebContentsStateBridge.getContentsStateAsByteBuffer(tab.getWebContents());
        } else {
            Referrer referrer = pendingLoadParams.getReferrer();
            return WebContentsStateBridge.createSingleNavigationStateAsByteBuffer(
                    pendingLoadParams.getUrl(), referrer != null ? referrer.getUrl() : null,
                    // Policy will be ignored for null referrer url, 0 is just a placeholder.
                    referrer != null ? referrer.getPolicy() : 0,
                    pendingLoadParams.getInitiatorOrigin(), tab.isIncognito());
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @Override
    public byte[] serialize() {
        try (TraceEvent e = TraceEvent.scoped("CriticalPersistedTabData.Serialize")) {
            WebContentsState webContentsState = mWebContentsState;
            if (webContentsState == null) {
                webContentsState = getWebContentsStateFromTab(mTab);
            }
            return CriticalPersistedTabDataProto.newBuilder()
                    .setParentId(mParentId)
                    .setRootId(mRootId)
                    .setTimestampMillis(mTimestampMillis)
                    .setWebContentsStateBytes(webContentsState == null
                                    ? ByteString.EMPTY
                                    : ByteString.copyFrom(
                                            getContentStateByteArray(webContentsState.buffer())))
                    .setContentStateVersion(mContentStateVersion)
                    .setOpenerAppId(mOpenerAppId)
                    .setThemeColor(mThemeColor)
                    .setLaunchTypeAtCreation(getLaunchType(mTabLaunchTypeAtCreation))
                    .build()
                    .toByteArray();
        }
    }

    protected static byte[] getContentStateByteArray(ByteBuffer buffer) {
        byte[] contentsStateBytes = new byte[buffer.limit()];
        buffer.rewind();
        buffer.get(contentsStateBytes);
        return contentsStateBytes;
    }

    // TODO(crbug.com/1113814) remove save() override
    @Override
    public void save() {
        if (mIsStorageRetrievalEnabled) {
            super.save();
        }
    }

    /**
     * Used in tests when we need to save a CriticalPersistedTabData object.
     * Ultimately will be deprecated by this.save() - for now this.save()
     * is not saving while fields are being migrated to CriticalPersistedTabData
     * TODO(crbug.com/1113813) Remove saveForTesting()
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void saveForTesting() {
        super.save();
    }

    /**
     * Delete {@link CriticalPersistedTabData} in storage
     */
    @Override
    public void delete() {
        if (mIsStorageRetrievalEnabled) {
            super.delete();
        }
    }

    @Override
    public void destroy() {}

    /**
     * @return title of the {@link Tab}
     */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @param title of the {@link Tab} to set
     */
    public void setTitle(String title) {
        mTitle = title;
        save();
    }

    /**
     * @return {@link GURL} for the {@link Tab}
     */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * Set {@link GURL} for the {@link Tab}
     * @param url to set
     */
    public void setUrl(GURL url) {
        mUrl = url;
        save();
    }

    /**
     * @return identifier for the {@link Tab}
     */
    public int getTabId() {
        return mTab.getId();
    }

    /**
     * @return root identifier for the {@link Tab}
     */
    public int getRootId() {
        return mRootId;
    }

    /**
     * Set root id
     */
    public void setRootId(int rootId) {
        if (mRootId == rootId) return;
        // TODO(crbug.com/1059640) add in setters for all mutable fields
        mRootId = rootId;
        for (CriticalPersistedTabDataObserver observer : mObservers) {
            observer.onRootIdChanged(mTab, rootId);
        }
        mTab.setIsTabStateDirty(true);
        save();
    }

    /**
     * @return parent identifier for the {@link Tab}
     */
    public int getParentId() {
        return mParentId;
    }

    /**
     * Set parent identifier for the {@link Tab}
     */
    public void setParentId(int parentId) {
        mParentId = parentId;
        save();
    }

    /**
     * @return timestamp in milliseconds for the {@link Tab}
     */
    public long getTimestampMillis() {
        return mTimestampMillis;
    }

    /**
     * set the timetsamp
     * @param timestamp the timestamp
     */
    public void setTimestampMillis(long timestamp) {
        mTimestampMillis = timestamp;
        save();
    }

    /**
     * @return content state bytes for the {@link Tab}
     */
    public WebContentsState getWebContentsState() {
        return mWebContentsState;
    }

    public void setWebContentsState(WebContentsState webContentsState) {
        mWebContentsState = webContentsState;
        save();
    }

    /**
     * @return content state version for the {@link Tab}
     */
    public int getContentStateVersion() {
        return mContentStateVersion;
    }

    /**
     * @return opener app id
     */
    public String getOpenerAppId() {
        return mOpenerAppId;
    }

    /**
     * @return theme color
     */
    public int getThemeColor() {
        return mThemeColor;
    }

    /**
     * @return launch type at creation
     */
    public @Nullable @TabLaunchType Integer getTabLaunchTypeAtCreation() {
        return mTabLaunchTypeAtCreation;
    }

    public void setLaunchTypeAtCreation(@Nullable @TabLaunchType Integer launchTypeAtCreation) {
        mTabLaunchTypeAtCreation = launchTypeAtCreation;
        save();
    }

    /**
     * Add a {@link CriticalPersistedTabDataObserver}
     * @param criticalPersistedTabDataObserver the observer
     */
    public void addObserver(CriticalPersistedTabDataObserver criticalPersistedTabDataObserver) {
        mObservers.addObserver(criticalPersistedTabDataObserver);
    }

    /**
     * Remove a {@link CriticalPersistedTabDataObserver}
     * @param criticalPersistedTabDataObserver the observer
     */
    public void removeObserver(CriticalPersistedTabDataObserver criticalPersistedTabDataObserver) {
        mObservers.removeObserver(criticalPersistedTabDataObserver);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void setIsStorageRetrievalEnabled(boolean isStorageRetrievalEnabled) {
        mIsStorageRetrievalEnabled = isStorageRetrievalEnabled;
    }
}
