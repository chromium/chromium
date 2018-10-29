// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.content.res.AppCompatResources;
import android.text.format.DateUtils;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.favicon.IconType;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.PersonalizedPromoViewHolder;
import org.chromium.chrome.browser.ntp.cards.SuggestionsCategoryInfo;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.SigninAccessPoint;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.suggestions.DestructionObserver;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsRanker;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.ThumbnailGradient;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProvider.ThumbnailRequest;
import org.chromium.chrome.browser.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.compositor.layouts.DisableChromeAnimations;
import org.chromium.chrome.test.util.browser.suggestions.DummySuggestionsEventReporter;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.net.NetworkChangeNotifier;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Tests for the appearance of Article Snippets.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ArticleSnippetsTest {
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    @Rule
    public TestRule mDisableChromeAnimations = new DisableChromeAnimations();

    private SuggestionsUiDelegate mUiDelegate;
    private FakeSuggestionsSource mSnippetsSource;
    private MockThumbnailProvider mThumbnailProvider;

    private SuggestionsRecyclerView mRecyclerView;
    private ContextMenuManager mContextMenuManager;
    private FrameLayout mContentView;
    private SnippetArticleViewHolder mSuggestion;
    private PersonalizedPromoViewHolder mSigninPromo;

    private UiConfig mUiConfig;

    private static final int FULL_CATEGORY = 0;
    private static final int MINIMAL_CATEGORY = 1;

    private long mTimestamp;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mThumbnailProvider = new MockThumbnailProvider();
        mSnippetsSource = new FakeSuggestionsSource();
        mSuggestionsDeps.getFactory().thumbnailProvider = mThumbnailProvider;
        mSuggestionsDeps.getFactory().suggestionsSource = mSnippetsSource;
        mUiDelegate = new MockUiDelegate();
        mSnippetsSource.setDefaultFavicon(getBitmap(R.drawable.star_green));

        mTimestamp = System.currentTimeMillis() - 5 * DateUtils.MINUTE_IN_MILLIS;

        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
        });

        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = mActivityTestRule.getActivity();
            mContentView = new FrameLayout(activity);
            mUiConfig = new UiConfig(mContentView);

            activity.setContentView(mContentView);

            mRecyclerView = new SuggestionsRecyclerView(activity);
            mContextMenuManager = new ContextMenuManager(mUiDelegate.getNavigationDelegate(),
                    mRecyclerView::setTouchEnabled, activity::closeContextMenu,
                    NewTabPage.CONTEXT_MENU_USER_ACTION_PREFIX);
            mRecyclerView.init(mUiConfig, mContextMenuManager);

            mSuggestion = new SnippetArticleViewHolder(mRecyclerView, mContextMenuManager,
                    mUiDelegate, mUiConfig, /* offlinePageBridge = */ null);
        });
    }

    @After
    public void tearDown() {
        if (mSigninPromo != null) mSigninPromo.setSigninPromoControllerForTests(null);
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testSnippetAppearance() throws IOException {
        SuggestionsCategoryInfo fullCategoryInfo = new SuggestionsCategoryInfo(FULL_CATEGORY,
                "Section Title", ContentSuggestionsCardLayout.FULL_CARD,
                ContentSuggestionsAdditionalAction.NONE,
                /* show_if_empty = */ true, "No suggestions");

        SnippetArticle shortSnippet = new SnippetArticle(FULL_CATEGORY, "id1", "Snippet",
                "Publisher", "www.google.com",
                mTimestamp, // Publish timestamp
                10f, // Score
                mTimestamp, // Fetch timestamp
                false, // Is video suggestion
                null); // Thumbnail dominant color
        Bitmap watch = BitmapFactory.decodeFile(
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/watch.jpg"));
        Drawable drawable = ThumbnailGradient.createDrawableWithGradientIfNeeded(
                watch, mActivityTestRule.getActivity().getResources());
        shortSnippet.setThumbnail(mUiDelegate.getReferencePool().put(drawable));

        renderSuggestion(shortSnippet, fullCategoryInfo, "short_snippet");

        SnippetArticle longSnippet = new SnippetArticle(FULL_CATEGORY, "id2",
                new String(new char[20]).replace("\0", "Snippet "),
                new String(new char[20]).replace("\0", "Publisher "),
                "www.google.com",
                mTimestamp, // Publish timestamp
                20f, // Score
                mTimestamp, // Fetch timestamp
                false, // Is video suggestion
                Color.GREEN); // Thumbnail dominant color
        renderSuggestion(longSnippet, fullCategoryInfo, "long_snippet");

        SuggestionsCategoryInfo minimalCategory = new SuggestionsCategoryInfo(MINIMAL_CATEGORY,
                "Section Title", ContentSuggestionsCardLayout.MINIMAL_CARD,
                ContentSuggestionsAdditionalAction.NONE,
                /* show_if_empty = */ true, "No suggestions");

        SnippetArticle minimalSnippet = new SnippetArticle(MINIMAL_CATEGORY, "id3",
                new String(new char[20]).replace("\0", "Bookmark "), "Publisher",
                "www.google.com",
                mTimestamp, // Publish timestamp
                10f, // Score
                mTimestamp, // Fetch timestamp
                false, // Is video suggestion
                null); // Thumbnail dominant color
        renderSuggestion(minimalSnippet, minimalCategory, "minimal_snippet");

        SnippetArticle minimalSnippet2 = new SnippetArticle(MINIMAL_CATEGORY, "id4", "Bookmark",
                "Publisher", "www.google.com",
                mTimestamp, // Publish timestamp
                10f, // Score
                mTimestamp, // Fetch timestamp
                false, // Is video suggestion
                null); // Thumbnail dominant color

        // See how everything looks in narrow layout.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Since we inform the UiConfig manually about the desired display style, the only
            // reason we actually change the LayoutParams is for the rendered Views to look right.
            ViewGroup.LayoutParams params = mContentView.getLayoutParams();
            params.width = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 350,
                    mRecyclerView.getResources().getDisplayMetrics());
            mContentView.setLayoutParams(params);

            mUiConfig.setDisplayStyleForTesting(new UiConfig.DisplayStyle(
                    HorizontalDisplayStyle.NARROW, VerticalDisplayStyle.REGULAR));
        });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        renderSuggestion(shortSnippet, fullCategoryInfo, "short_snippet_narrow");
        renderSuggestion(longSnippet, fullCategoryInfo, "long_snippet_narrow");
        renderSuggestion(minimalSnippet, minimalCategory, "long_minimal_snippet_narrow");
        renderSuggestion(minimalSnippet2, minimalCategory, "short_minimal_snippet_narrow");
    }

    // TODO(bauerb): Test top, middle, and bottom card backgrounds.

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testDownloadSuggestion() throws IOException {
        String downloadFilePath =
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/capybara.jpg");
        ThreadUtils.runOnUiThreadBlocking(() -> {
            SnippetArticle downloadSuggestion = new SnippetArticle(KnownCategories.DOWNLOADS, "id1",
                    "test_image.jpg", "example.com", "http://example.com",
                    mTimestamp, // Publish timestamp
                    10f, // Score
                    mTimestamp, // Fetch timestamp
                    false, // Is video suggestion
                    null); // Thumbnail dominant color
            downloadSuggestion.setAssetDownloadData("asdf", downloadFilePath, "image/jpeg");
            SuggestionsCategoryInfo downloadsCategory = new SuggestionsCategoryInfo(
                    KnownCategories.DOWNLOADS, "Downloads", ContentSuggestionsCardLayout.FULL_CARD,
                    ContentSuggestionsAdditionalAction.NONE,
                    /* show_if_empty = */ true, "No suggestions");

            mSuggestion.onBindViewHolder(downloadSuggestion, downloadsCategory);
            mContentView.addView(mSuggestion.itemView);
        });

        mRenderTestRule.render(mSuggestion.itemView, "download_snippet_placeholder");

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        List<ThumbnailRequest> requests = mThumbnailProvider.getRequests();
        Assert.assertEquals(1, requests.size());
        ThumbnailRequest request = requests.get(0);
        Assert.assertEquals(downloadFilePath, request.getFilePath());

        Bitmap thumbnail = BitmapFactory.decodeFile(downloadFilePath);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mThumbnailProvider.fulfillRequest(request, thumbnail);
        });

        mRenderTestRule.render(mSuggestion.itemView, "download_snippet_thumbnail");
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testVideoSuggestion() throws IOException {
        SuggestionsCategoryInfo categoryInfo = new SuggestionsCategoryInfo(FULL_CATEGORY,
                "Section Title", ContentSuggestionsCardLayout.FULL_CARD,
                ContentSuggestionsAdditionalAction.NONE,
                /* show_if_empty = */ true, "No suggestions");

        SnippetArticle suggestionWithLightDominantColor =
                new SnippetArticle(FULL_CATEGORY, "id1", "Snippet", "Publisher", "www.google.com",
                        mTimestamp, // Publish timestamp
                        10f, // Score
                        mTimestamp, // Fetch timestamp
                        true, // Is video suggestion
                        0xFFEEEEEE); // Thumbnail dominant color
        renderSuggestion(suggestionWithLightDominantColor, categoryInfo,
                "video_suggestion_with_light_dominant_color");

        SnippetArticle suggestionWithLightThumbnail =
                new SnippetArticle(FULL_CATEGORY, "id1", "Snippet", "Publisher", "www.google.com",
                        mTimestamp, // Publish timestamp
                        10f, // Score
                        mTimestamp, // Fetch timestamp
                        true, // Is video suggestion
                        0xFFEEEEEE); // Thumbnail dominant color
        setThumbnail(suggestionWithLightThumbnail, "chrome/test/data/android/watch.jpg");
        renderSuggestion(suggestionWithLightThumbnail, categoryInfo,
                "video_suggestion_with_light_thumbnail");

        SnippetArticle suggestionWithDarkDominantColor =
                new SnippetArticle(FULL_CATEGORY, "id1", "Snippet", "Publisher", "www.google.com",
                        mTimestamp, // Publish timestamp
                        10f, // Score
                        mTimestamp, // Fetch timestamp
                        true, // Is video suggestion
                        0xFF8E5C39); // Thumbnail dominant color
        renderSuggestion(suggestionWithDarkDominantColor, categoryInfo,
                "video_suggestion_with_dark_dominant_color");

        SnippetArticle suggestionWithDarkThumbnail =
                new SnippetArticle(FULL_CATEGORY, "id1", "Snippet", "Publisher", "www.google.com",
                        mTimestamp, // Publish timestamp
                        10f, // Score
                        mTimestamp, // Fetch timestamp
                        true, // Is video suggestion
                        0xFF8E5C39); // Thumbnail dominant color
        setThumbnail(suggestionWithDarkThumbnail, "chrome/test/data/android/capybara.jpg");
        renderSuggestion(
                suggestionWithDarkThumbnail, categoryInfo, "video_suggestion_with_dark_thumbnail");
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testPersonalizedSigninPromosNoAccounts() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            createPersonalizedSigninPromo(null);
            mContentView.addView(mSigninPromo.itemView);
        });
        mRenderTestRule.render(mSigninPromo.itemView, "cold_state_personalized_signin_promo");
    }

    @Test
    @MediumTest
    @Feature({"ArticleSnippets", "RenderTest"})
    public void testPersonalizedSigninPromosWithAccount() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            createPersonalizedSigninPromo(getTestProfileData());
            mContentView.addView(mSigninPromo.itemView);
        });
        mRenderTestRule.render(mSigninPromo.itemView, "hot_state_personalized_signin_promo");
    }

    private void createPersonalizedSigninPromo(@Nullable DisplayableProfileData profileData) {
        SigninPromoController signinPromoController =
                new SigninPromoController(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);
        mSigninPromo = new PersonalizedPromoViewHolder(mRecyclerView, null, mUiConfig);
        mSigninPromo.setSigninPromoControllerForTests(signinPromoController);
        mSigninPromo.bindAndConfigureViewForTests(profileData);
    }

    private DisplayableProfileData getTestProfileData() {
        String accountId = "test@gmail.com";
        Drawable image = AppCompatResources.getDrawable(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                R.drawable.logo_avatar_anonymous);
        String fullName = "Test Account";
        String givenName = "Test";
        return new DisplayableProfileData(accountId, image, fullName, givenName);
    }

    private void renderSuggestion(SnippetArticle suggestion, SuggestionsCategoryInfo categoryInfo,
            String renderId) throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSuggestion.onBindViewHolder(suggestion, categoryInfo);
            mContentView.addView(mSuggestion.itemView);
        });
        mRenderTestRule.render(mSuggestion.itemView, renderId);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView.removeView(mSuggestion.itemView);
            mSuggestion.recycle();
        });
    }

    private void setThumbnail(SnippetArticle suggestion, String thumbnailPath) {
        Bitmap watch = BitmapFactory.decodeFile(UrlUtils.getIsolatedTestFilePath(thumbnailPath));
        Drawable drawable = ThumbnailGradient.createDrawableWithGradientIfNeeded(
                watch, mActivityTestRule.getActivity().getResources());
        suggestion.setThumbnail(mUiDelegate.getReferencePool().put(drawable));
    }

    private Bitmap getBitmap(@DrawableRes int resId) {
        return BitmapFactory.decodeResource(
                InstrumentationRegistry.getInstrumentation().getTargetContext().getResources(),
                resId);
    }

    /**
     * Simple mock ThumbnailProvider that allows delaying requests and fulfilling them at a later
     * point.
     */
    private static class MockThumbnailProvider implements ThumbnailProvider {
        private final List<ThumbnailRequest> mRequests = new ArrayList<>();

        public List<ThumbnailRequest> getRequests() {
            return mRequests;
        }

        public void fulfillRequest(ThumbnailRequest request, Bitmap bitmap) {
            cancelRetrieval(request);
            request.onThumbnailRetrieved(request.getFilePath(), bitmap);
        }

        @Override
        public void destroy() {}

        @Override
        public void getThumbnail(ThumbnailRequest request) {
            mRequests.add(request);
        }

        @Override
        public void removeThumbnailsFromDisk(String contentId) {}

        @Override
        public void cancelRetrieval(ThumbnailRequest request) {
            boolean removed = mRequests.remove(request);
            Assert.assertTrue(
                    String.format(Locale.US, "Request for '%s' not found", request.getFilePath()),
                    removed);
        }
    }

    /**
     * A SuggestionsUiDelegate to initialize our Adapter.
     */
    private class MockUiDelegate implements SuggestionsUiDelegate {
        private final SuggestionsEventReporter mSuggestionsEventReporter =
                new DummySuggestionsEventReporter();
        private final SuggestionsRanker mSuggestionsRanker = new SuggestionsRanker();
        private final DiscardableReferencePool mReferencePool = new DiscardableReferencePool();
        private final ImageFetcher mImageFetcher =
                new MockImageFetcher(mSnippetsSource, mReferencePool);

        @Override
        public SuggestionsSource getSuggestionsSource() {
            return mSnippetsSource;
        }

        @Override
        public SuggestionsRanker getSuggestionsRanker() {
            return mSuggestionsRanker;
        }

        @Override
        public DiscardableReferencePool getReferencePool() {
            return mReferencePool;
        }

        @Override
        public void addDestructionObserver(DestructionObserver destructionObserver) {}

        @Override
        public boolean isVisible() {
            return true;
        }

        @Override
        public SuggestionsEventReporter getEventReporter() {
            return mSuggestionsEventReporter;
        }

        @Override
        public SuggestionsNavigationDelegate getNavigationDelegate() {
            return null;
        }

        @Override
        public ImageFetcher getImageFetcher() {
            return mImageFetcher;
        }

        @Override
        public SnackbarManager getSnackbarManager() {
            return mActivityTestRule.getActivity().getSnackbarManager();
        }
    }

    private class MockImageFetcher extends ImageFetcher {
        public MockImageFetcher(
                SuggestionsSource suggestionsSource, DiscardableReferencePool referencePool) {
            super(suggestionsSource, null, referencePool, null);
        }

        @Override
        public void makeFaviconRequest(SnippetArticle suggestion, final int faviconSizePx,
                final Callback<Bitmap> faviconCallback) {
            // Run the callback asynchronously in case the caller made that assumption.
            ThreadUtils.postOnUiThread(() -> {
                // Return an arbitrary drawable.
                faviconCallback.onResult(getBitmap(R.drawable.star_green));
            });
        }

        @Override
        public void makeLargeIconRequest(final String url, final int largeIconSizePx,
                final LargeIconBridge.LargeIconCallback callback) {
            // Run the callback asynchronously in case the caller made that assumption.
            ThreadUtils.postOnUiThread(() -> {
                // Return an arbitrary drawable.
                callback.onLargeIconAvailable(
                        getBitmap(R.drawable.star_green), largeIconSizePx, true, IconType.INVALID);
            });
        }
    }
}
