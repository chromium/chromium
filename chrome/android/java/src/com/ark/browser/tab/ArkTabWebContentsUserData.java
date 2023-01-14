package com.ark.browser.tab;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.ArkWebContents;

import org.chromium.base.UserData;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * UserData for a {@link Tab}. Used for a {@link WebContents} while it stays
 * active for the Tab.
 */
public abstract class ArkTabWebContentsUserData implements UserData {
    private ArkWebContents mWebContents;

    public ArkTabWebContentsUserData(Tab tab) {
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                ArkTabImpl arkTab = (ArkTabImpl) tab;
                if (mWebContents == arkTab.getArkWeb()) return;
                if (mWebContents != null) cleanupWebContents(mWebContents);
                mWebContents = arkTab.getArkWeb();
                if (mWebContents != null) initWebContents(mWebContents);
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                // Intentionally do nothing to prevent automatic observer removal on detachment.
            }

            @Override
            public void onAttachToWindowAndroid(Tab tab, @NonNull WindowAndroid windowAndroid) {
                ArkTabWebContentsUserData.this.onAttachToWindowAndroid(windowAndroid);
            }

            @Override
            public void onDetachToWindowAndroid(Tab tab, @NonNull WindowAndroid windowAndroid) {
                ArkTabWebContentsUserData.this.onDetachToWindowAndroid();
            }
        });
    }

    @Override
    public final void destroy() {
        cleanupWebContents(mWebContents);
        destroyInternal();
    }

    protected ArkWebContents getWebContents() {
        return mWebContents;
    }

    /**
     * Performs additional tasks upon destruction.
     */
    protected void destroyInternal() {}

    /**
     * Called when {@link WebContents} becomes active (swapped in) for a {@link Tab}.
     * @param webContents WebContents object that just became active.
     */
    public abstract void initWebContents(ArkWebContents webContents);

    /**
     * Called when {@link WebContents} gets swapped out.
     * @param webContents WebContents object that just became inactive.
     */
    public abstract void cleanupWebContents(ArkWebContents webContents);

    public void onAttachToWindowAndroid(@NonNull WindowAndroid windowAndroid) {

    }

    public void onDetachToWindowAndroid() {

    }
}
