// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.cryptids;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;

/**
 * Allows for cryptids to be displayed on the New Tab Page under certain probabilistic conditions.
 */
public class ProbabilisticCryptidRenderer {
    // Probabilities are expressed as instances per million.
    protected static final int DENOMINATOR = 1000000;

    private static final String MORATORIUM_LENGTH_PARAM_NAME = "moratorium-length-millis";
    private static final String RAMP_UP_LENGTH_PARAM_NAME = "ramp-up-length-millis";
    private static final String MAX_PROBABILITY_PARAM_NAME = "max-probability-per-million";

    private static final String URL_FORMAT = "https://www.gstatic.com/chrome/cryptids/%s.gif";
    private static final String ASSET_KEY_PARAM_NAME = "asset-key";
    private static final String EMPTY_ASSET_KEY = "empty";

    private static final long ONE_DAY = 24 * 60 * 60 * 1000;
    private static final long DEFAULT_MORATORIUM_LENGTH = 4 * ONE_DAY;
    private static final long DEFAULT_RAMP_UP_LENGTH = 21 * ONE_DAY;
    private static final int DEFAULT_MAX_PROBABILITY = 20000; // 2%

    // Values correspond to CryptidRenderResult in enums.xml
    private static final int HISTOGRAM_SUCCESS = 0;
    private static final int HISTOGRAM_FAILURE = 1;
    private static final int HISTOGRAM_DISABLED = 2;
    private static final int HISTOGRAM_MAX = 3;

    private static final String TAG = "ProbabilisticCryptid";

    private static ProbabilisticCryptidRenderer sInstance = new ProbabilisticCryptidRenderer();

    public static ProbabilisticCryptidRenderer getInstance() {
        return sInstance;
    }

    /**
     * Determines whether cryptid should be rendered on this NTP instance, based on probability
     * factors as well as certain restrictions (managed non-null profile, incognito, or disabled
     * Feature flag will prevent display).
     * @param profile the current user profile. Should not be null, except in tests.
     * @return true if the conditions are met and cryptid should be shown, false otherwise
     */
    public boolean shouldUseCryptidRendering(Profile profile) {
        // Profile may be null for testing.
        if (profile != null && isBlocked(profile)) {
            return false;
        } else if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PROBABILISTIC_CRYPTID_RENDERER)) {
            return false;
        }

        return getRandom() < calculateProbability();
    }

    /** Records the current timestamp as the last render time in the appropriate pref. */
    public void recordRenderEvent() {
        recordRenderEvent(System.currentTimeMillis());
    }

    /**
     * Creates an Drawable which will render a cryptid, suitable for use in/around the space of the
     * logo on the NTP.
     * @param profile the current user profile. Should not be null, except in tests.
     * @param callback A callback which will insert this Drawable into an ImageView in the
     *         appropriate
     *     location. The callback should handle null Drawables, which will be passed if a cryptid is
     *     not available or shouldn't be used.
     */
    public void getCryptidForLogo(Profile profile, Callback<Drawable> callback) {
        if (!shouldUseCryptidRendering(profile)) {
            RecordHistogram.recordEnumeratedHistogram(
                    "NewTabPage.CryptidRenderResult", HISTOGRAM_DISABLED, HISTOGRAM_MAX);
            callback.onResult(null);
            return;
        }

        ImageFetcher fetcher =
                ImageFetcherFactory.createImageFetcher(
                        ImageFetcherConfig.DISK_CACHE_ONLY, profile.getProfileKey());

        // If no asset key is provided, an empty asset with key "empty" will be fetched; this allows
        // end-to-end execution without any visible change. To test with visible changes, the asset
        // key "test" should be provided.
        fetcher.fetchGif(
                ImageFetcher.Params.create(
                        String.format(
                                URL_FORMAT, getParamValue(ASSET_KEY_PARAM_NAME, EMPTY_ASSET_KEY)),
                        ImageFetcher.CRYPTIDS_UMA_CLIENT_NAME),
                new Callback<BaseGifImage>() {
                    @Override
                    public void onResult(BaseGifImage image) {
                        fetcher.destroy();

                        if (image == null) {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "NewTabPage.CryptidRenderResult",
                                    HISTOGRAM_FAILURE,
                                    HISTOGRAM_MAX);
                            callback.onResult(null);
                            return;
                        }

                        BaseGifDrawable drawable =
                                new BaseGifDrawable(image, Bitmap.Config.ARGB_8888);
                        drawable.setLoopCount(1); // Plays only once/does not loop.
                        drawable.setAnimateOnLoad(true);
                        callback.onResult(drawable);
                    }
                });
    }

    // Protected for testing
    protected long getLastRenderTimestampMillis() {
        long lastRenderTimestamp =
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys.CRYPTID_LAST_RENDER_TIMESTAMP,
                                /* defaultValue= */ 0);
        if (lastRenderTimestamp == 0) {
            // If no render has ever been recorded, set the timestamp such that the current time is
            // the end of the moratorium period.
            lastRenderTimestamp = System.currentTimeMillis() - getRenderingMoratoriumLengthMillis();
            recordRenderEvent(lastRenderTimestamp);
        }
        return lastRenderTimestamp;
    }

    protected long getRenderingMoratoriumLengthMillis() {
        return getParamValue(MORATORIUM_LENGTH_PARAM_NAME, DEFAULT_MORATORIUM_LENGTH);
    }

    protected long getRampUpLengthMillis() {
        return getParamValue(RAMP_UP_LENGTH_PARAM_NAME, DEFAULT_RAMP_UP_LENGTH);
    }

    protected int getMaxProbability() {
        return getParamValue(MAX_PROBABILITY_PARAM_NAME, DEFAULT_MAX_PROBABILITY);
    }

    protected int getRandom() {
        return (int) (Math.random() * DENOMINATOR);
    }

    @VisibleForTesting
    void recordRenderEvent(long timestamp) {
        RecordUserAction.record("CryptidRendered");
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.CryptidRenderResult", HISTOGRAM_SUCCESS, HISTOGRAM_MAX);
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.CRYPTID_LAST_RENDER_TIMESTAMP, timestamp);
    }

    /**
     * Calculates the probability of display at a particular moment, based on various timestamp/
     * length factors. Roughly speaking, the probability starts at 0 after a rendering event,
     * then after a moratorium period will ramp up linearly until reaching a maximum probability.
     * @param lastRenderTimestamp the stored timestamp, in millis, of the last rendering event
     * @param currentTimestamp the current time, in millis
     * @param renderingMoratoriumLength the length, in millis, of the period prior to ramp-up
     *     when probability should remain at zero
     * @param rampUpLength the length, in millis, of the period over which the linear ramp-up
     *     should occur
     * @param maxProbability the highest probability value, as a fraction of |DENOMINATOR|, that
     *     should ever be returned (i.e., the value at the end of the ramp-up period)
     * @return the probability, expressed as a fraction of |DENOMINATOR|, that cryptids will be
     *     rendered
     */
    @VisibleForTesting
    static int calculateProbability(
            long lastRenderTimestamp,
            long currentTimestamp,
            long renderingMoratoriumLength,
            long rampUpLength,
            int maxProbability) {
        if (currentTimestamp < lastRenderTimestamp) {
            Log.e(TAG, "Last render timestamp is in the future");
            return 0;
        }

        long windowStartTimestamp = lastRenderTimestamp + renderingMoratoriumLength;
        long maxProbabilityStartTimestamp = windowStartTimestamp + rampUpLength;

        if (currentTimestamp < windowStartTimestamp) {
            return 0;
        } else if (currentTimestamp > maxProbabilityStartTimestamp) {
            return maxProbability;
        }

        float fractionOfRampUp = (float) (currentTimestamp - windowStartTimestamp) / rampUpLength;
        return (int) Math.round(fractionOfRampUp * maxProbability);
    }

    /** Convenience method calling the static method above using the appropriate values. */
    @VisibleForTesting
    int calculateProbability() {
        return calculateProbability(
                getLastRenderTimestampMillis(),
                System.currentTimeMillis(),
                getRenderingMoratoriumLengthMillis(),
                getRampUpLengthMillis(),
                getMaxProbability());
    }

    /**
     * Enforces that feature is not used in blocked contexts (namely, incognito/OTR, and when
     * enterprise policies are active).
     */
    private boolean isBlocked(Profile profile) {
        return profile.isOffTheRecord() || ManagedBrowserUtils.isBrowserManaged(profile);
    }

    /**
     * Helper method for getting a Feature param associated with this class's Feature, with type
     * String. Returns the provided default if no such param value is set.
     */
    private String getParamValue(String name, String defaultValue) {
        String paramVal =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PROBABILISTIC_CRYPTID_RENDERER, name);
        return paramVal.length() > 0 ? paramVal : defaultValue;
    }

    /**
     * Helper method for getting and parsing a Feature param associated with this class's Feature,
     * with type long. Returns the provided default if no such param value is set, or if the value
     * provided can't be parsed as a long (in which case an error is also logged).
     */
    private long getParamValue(String name, long defaultValue) {
        String paramVal =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PROBABILISTIC_CRYPTID_RENDERER, name);
        try {
            if (paramVal.length() > 0) {
                return Long.parseLong(paramVal);
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, String.format("Invalid long value %s for param %s", paramVal, name), e);
        }

        return defaultValue;
    }

    /**
     * Helper method for getting and parsing a Feature param associated with this class's Feature,
     * with type int. Returns the provided default if no such param value is set, or if the value
     * provided can't be parsed as a int (in which case an error is also logged).
     */
    private int getParamValue(String name, int defaultValue) {
        String paramVal =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.PROBABILISTIC_CRYPTID_RENDERER, name);
        try {
            if (paramVal.length() > 0) {
                return Integer.parseInt(paramVal);
            }
        } catch (NumberFormatException e) {
            Log.e(TAG, String.format("Invalid int value %s for param %s", paramVal, name), e);
        }

        return defaultValue;
    }
}
