// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.installedapp;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Bundle;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.installedapp.mojom.InstalledAppProvider;
import org.chromium.installedapp.mojom.RelatedApplication;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.List;

/** Ensure that the InstalledAppProvider returns the correct apps. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class InstalledAppProviderTest {
    private static final String ASSET_STATEMENTS_KEY =
            InstalledAppProviderImpl.ASSET_STATEMENTS_KEY;
    private static final String RELATION_HANDLE_ALL_URLS =
            "delegate_permission/common.handle_all_urls";
    private static final String NAMESPACE_WEB =
            InstalledAppProviderImpl.ASSET_STATEMENT_NAMESPACE_WEB;
    private static final String PLATFORM_ANDROID =
            InstalledAppProviderImpl.RELATED_APP_PLATFORM_ANDROID;
    private static final String PLATFORM_OTHER = "itunes";
    // Note: Android package name and origin deliberately unrelated (there is no requirement that
    // they be the same).
    private static final String PACKAGE_NAME_1 = "com.app1.package";
    private static final String PACKAGE_NAME_2 = "com.app2.package";
    private static final String PACKAGE_NAME_3 = "com.app3.package";
    private static final String URL_UNRELATED = "https://appstore.example.com/app1";
    private static final String ORIGIN = "https://example.com:8000";
    private static final String URL_ON_ORIGIN =
            "https://example.com:8000/path/to/page.html?key=value#fragment";
    private static final String ORIGIN_SYNTAX_ERROR = "https:{";
    private static final String ORIGIN_MISSING_SCHEME = "path/only";
    private static final String ORIGIN_MISSING_HOST = "file:///path/piece";
    private static final String ORIGIN_MISSING_PORT = "http://example.com";
    private static final String ORIGIN_DIFFERENT_SCHEME = "http://example.com:8000";
    private static final String ORIGIN_DIFFERENT_HOST = "https://example.org:8000";
    private static final String ORIGIN_DIFFERENT_PORT = "https://example.com:8001";

    private FakeFrameUrlDelegate mFrameUrlDelegate;
    private InstalledAppProviderTestImpl mInstalledAppProvider;
    private FakeInstantAppsHandler mFakeInstantAppsHandler;

    private static class InstalledAppProviderTestImpl extends InstalledAppProviderImpl {
        private long mLastDelayMillis;

        public InstalledAppProviderTestImpl(FrameUrlDelegate frameUrlDelegate, Context context,
                FakeInstantAppsHandler instantAppsHandler) {
            super(frameUrlDelegate, context, instantAppsHandler);
        }

        public long getLastDelayMillis() {
            return mLastDelayMillis;
        }

        @Override
        protected void delayThenRun(Runnable r, long delayMillis) {
            mLastDelayMillis = delayMillis;
            r.run();
        }
    }

    /**
     * FakeInstantAppsHandler lets us mock getting RelatedApplications from a URL in the absence of
     * proper GMSCore calls.
     */
    private static class FakeInstantAppsHandler extends InstantAppsHandler {
        private final List<Pair<String, Boolean>> mRelatedApplicationList;

        public FakeInstantAppsHandler() {
            mRelatedApplicationList = new ArrayList<Pair<String, Boolean>>();
        }

        public void addInstantApp(String url, boolean holdback) {
            mRelatedApplicationList.add(Pair.create(url, holdback));
        }

        public void resetForTest() {
            mRelatedApplicationList.clear();
        }

        // TODO(thildebr): When the implementation of isInstantAppAvailable is complete, we need to
        // test its functionality instead of stubbing it out here. Instead we can create a wrapper
        // around the GMSCore functionality we need and override that here instead.
        @Override
        public boolean isInstantAppAvailable(
                String url, boolean checkHoldback, boolean includeUserPrefersBrowser) {
            for (Pair<String, Boolean> pair : mRelatedApplicationList) {
                if (url.startsWith(pair.first) && checkHoldback == pair.second) {
                    return true;
                }
            }
            return false;
        }
    }

    /**
     * Helper function allows for the "installation" of Android package names and setting up
     * Resources for installed packages.
     */
    private void setMetaDataAndResourcesForTest(
            String packageName, Bundle metaData, Resources resources) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.metaData = metaData;

        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        packageManager.addPackage(packageInfo);
        packageManager.resources.put(packageInfo.packageName, resources);
    }

    /**
     * Fakes the Resources object, allowing lookup of a single String value.
     *
     * <p>Note: The real Resources object defines a mapping to many values. This fake object only
     * allows a single value in the mapping, and it must be a String (which is all that is required
     * for these tests).
     */
    private static class FakeResources extends Resources {
        private final int mId;
        private final String mValue;

        // Do not warn about deprecated call to Resources(); the documentation says code is not
        // supposed to create its own Resources object, but we are using it to fake out the
        // Resources, and there is no other way to do that.
        @SuppressWarnings("deprecation")
        public FakeResources(int identifier, String value) {
            super(new AssetManager(), null, null);
            mId = identifier;
            mValue = value;
        }

        @Override
        public int getIdentifier(String name, String defType, String defPackage) {
            if (name == null) throw new NullPointerException();

            // There is *no guarantee* (in the Digital Asset Links spec) about what the string
            // resource should be called ("asset_statements" is just an example). Therefore,
            // getIdentifier cannot be used to get the asset statements string. Always fail the
            // lookup here, to ensure the implementation isn't relying on any particular hard-coded
            // string.
            return 0;
        }

        @Override
        public String getString(int id) {
            if (id != mId) {
                throw new Resources.NotFoundException("id 0x" + Integer.toHexString(id));
            }

            return mValue;
        }
    }

    private static final class FakeFrameUrlDelegate
            implements InstalledAppProviderImpl.FrameUrlDelegate {
        private URI mFrameUrl;
        private boolean mIncognito;

        public FakeFrameUrlDelegate(String frameUrl) {
            setFrameUrl(frameUrl);
        }

        public void setFrameUrl(String frameUrl) {
            if (frameUrl == null) {
                mFrameUrl = null;
                return;
            }

            try {
                mFrameUrl = new URI(frameUrl);
            } catch (URISyntaxException e) {
                throw new AssertionError(e);
            }
        }

        @Override
        public URI getUrl() {
            return mFrameUrl;
        }

        public void setIncognito(boolean incognito) {
            mIncognito = incognito;
        }

        @Override
        public boolean isIncognito() {
            return mIncognito;
        }
    }

    /** Creates a metaData bundle with a single resource-id key. */
    private static Bundle createMetaData(String metaDataName, int metaDataResourceId) {
        Bundle metaData = new Bundle();
        metaData.putInt(metaDataName, metaDataResourceId);
        return metaData;
    }

    /**
     * Sets a resource with a single key-value pair in an Android package's manifest.
     *
     * <p>The value is always a string.
     */
    private void setStringResource(String packageName, String key, String value) {
        int identifier = 0x1234;
        Bundle metaData = createMetaData(key, identifier);
        FakeResources resources = new FakeResources(identifier, value);
        setMetaDataAndResourcesForTest(packageName, metaData, resources);
    }

    /** Creates a valid Android asset statement string. */
    private String createAssetStatement(String platform, String relation, String url) {
        return String.format(
                "{\"relation\": [\"%s\"], \"target\": {\"namespace\": \"%s\", \"site\": \"%s\"}}",
                relation, platform, url);
    }

    /**
     * Sets an asset statement to an Android package's manifest (in the fake package manager).
     *
     * <p>Only one asset statement can be set for a given package (if this is called twice on the
     * same package, overwrites the previous asset statement).
     *
     * <p>This corresponds to a Statement List in the Digital Asset Links spec v1.
     */
    private void setAssetStatement(
            String packageName, String platform, String relation, String url) {
        String statements = "[" + createAssetStatement(platform, relation, url) + "]";
        setStringResource(packageName, ASSET_STATEMENTS_KEY, statements);
    }

    /** Creates a RelatedApplication to put in the web app manifest. */
    private RelatedApplication createRelatedApplication(String platform, String id, String url) {
        RelatedApplication application = new RelatedApplication();
        application.platform = platform;
        application.id = id;
        application.url = url;
        return application;
    }

    /**
     * Calls filterInstalledApps with the given inputs, and tests that the expected result is
     * returned.
     */
    private void verifyInstalledApps(RelatedApplication[] manifestRelatedApps,
            RelatedApplication[] expectedInstalledRelatedApps) {
        mInstalledAppProvider.filterInstalledApps(
                manifestRelatedApps, new InstalledAppProvider.FilterInstalledAppsResponse() {
                    @Override
                    public void call(RelatedApplication[] installedRelatedApps) {
                        Assert.assertEquals(
                                expectedInstalledRelatedApps.length, installedRelatedApps.length);

                        for (int i = 0; i < installedRelatedApps.length; i++) {
                            Assert.assertEquals(
                                    expectedInstalledRelatedApps[i], installedRelatedApps[i]);
                        }
                    }
                });
    }

    @Before
    public void setUp() {
        // Avoid triggering asserts in InstalledAppProviderImpl that check they are being run off
        // the UI thread (since this is a single-threaded test).
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mFrameUrlDelegate = new FakeFrameUrlDelegate(URL_ON_ORIGIN);
        mFakeInstantAppsHandler = new FakeInstantAppsHandler();
        mInstalledAppProvider = new InstalledAppProviderTestImpl(
                mFrameUrlDelegate, RuntimeEnvironment.application, mFakeInstantAppsHandler);
    }

    @After
    public void tearDown() {
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
    }

    /** Origin of the page using the API is missing certain parts of the URI. */
    @Test
    @Feature({"InstalledApp"})
    public void testOriginMissingParts() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};

        mFrameUrlDelegate.setFrameUrl(ORIGIN_MISSING_SCHEME);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        mFrameUrlDelegate.setFrameUrl(ORIGIN_MISSING_HOST);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Incognito mode with one related Android app. */
    @Test
    @Feature({"InstalledApp"})
    public void testIncognitoWithOneInstalledRelatedApp() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};

        mFrameUrlDelegate.setIncognito(true);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * No related Android apps.
     *
     * <p>An Android app relates to the web app, but not mutual.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testNoRelatedApps() {
        // The web manifest has no related apps.
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {};

        // One Android app is installed named |PACKAGE_NAME_1|. It has a related web app with origin
        // |ORIGIN|.
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app with no id (package name).
     *
     * <p>An Android app relates to the web app, but not mutual.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppNoId() {
        RelatedApplication manifestRelatedApps[] =
                new RelatedApplication[] {createRelatedApplication(PLATFORM_ANDROID, null, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related app (from a non-Android platform).
     *
     * <p>An Android app with the same id relates to the web app. This should be ignored since the
     * manifest doesn't mention the Android app.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedNonAndroidApp() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_OTHER, PACKAGE_NAME_1, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is not installed.
     *
     * <p>Another Android app relates to the web app, but not mutual.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppNotInstalled() {
        // The web manifest has a related Android app named |PACKAGE_NAME_1|.
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        // One Android app is installed named |PACKAGE_NAME_2|. It has a related web app with origin
        // |ORIGIN|.
        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app manifest has an asset_statements key, but the resource it links to is missing.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppBrokenAssetStatementsResource() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        Bundle metaData = createMetaData(ASSET_STATEMENTS_KEY, 0x1234);
        String statements =
                "[" + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN) + "]";
        FakeResources resources = new FakeResources(0x4321, statements);
        setMetaDataAndResourcesForTest(PACKAGE_NAME_1, metaData, resources);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is not mutually related (has no asset_statements). */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppNoAssetStatements() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setStringResource(PACKAGE_NAME_1, null, null);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is not mutually related (has no asset_statements). */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppNoAssetStatementsNullMetadata() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        FakeResources resources = new FakeResources(0x4321, null);
        setMetaDataAndResourcesForTest(PACKAGE_NAME_1, null, resources);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is related to other origins.
     *
     * <p>Tests three cases: - The Android app is related to a web app with a different scheme. -
     * The Android app is related to a web app with a different host. - The Android app is related
     * to a web app with a different port.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testOneRelatedAppRelatedToDifferentOrigins() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_SCHEME);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_HOST);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_PORT);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is installed and mutually related. */
    @Test
    @Feature({"InstalledApp"})
    public void testOneInstalledRelatedApp() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Change the frame URL and ensure the app relates to the new URL, not the old one.
     *
     * <p>This simulates navigating the frame while keeping the same Mojo service open.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testDynamicallyChangingUrl() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_SCHEME);

        // Should be empty, since Android app does not relate to this frame's origin.
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        // Simulate a navigation to a different origin.
        mFrameUrlDelegate.setFrameUrl(ORIGIN_DIFFERENT_SCHEME);

        // Now the result should include the Android app that relates to the new origin.
        expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        // Simulate the native RenderFrameHost disappearing.
        mFrameUrlDelegate.setFrameUrl(null);

        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app (installed and mutually related), with a non-null URL field. */
    @Test
    @Feature({"InstalledApp"})
    public void testInstalledRelatedAppWithUrl() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, URL_UNRELATED)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** One related Android app; Android app is related to multiple origins. */
    @Test
    @Feature({"InstalledApp"})
    public void testMultipleAssetStatements() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        // Create an asset_statements field with multiple statements. The second one matches the web
        // app.
        String statements = "["
                + createAssetStatement(
                          NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_DIFFERENT_HOST)
                + ", " + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN)
                + "]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** A JSON syntax error in the Android app's asset statement. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementSyntaxError() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements = "[{\"target\" {}}]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** The Android app's asset statement is not an array. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNotArray() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statement = createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statement);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** The Android app's asset statement array contains non-objects. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementArrayNoObjects() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements = "["
                + createAssetStatement(NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN) + ", 4]";
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        // Expect it to ignore the integer and successfully parse the valid object.
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Android app has no "relation" in the asset statement.
     *
     * <p>Currently, the relation string (in the Android package's asset statement) is ignored, so
     * the app is still returned as "installed".
     */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNoRelation() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements = String.format(
                "[{\"target\": {\"namespace\": \"%s\", \"site\": \"%s\"}}]", NAMESPACE_WEB, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather than any
        // or no relation?
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Android app is related with a non-standard relation.
     *
     * <p>Currently, the relation string (in the Android package's asset statement) is ignored, so
     * any will do. Is this desirable, or do we want to require a specific relation string?
     */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNonStandardRelation() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, "nonstandard/relation", ORIGIN);

        // TODO(mgiuca): [Spec issue] Should we require a specific relation string, rather than any
        // or no relation?
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "target" in the asset statement. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNoTarget() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements = String.format("[{\"relation\": [\"%s\"]}]", RELATION_HANDLE_ALL_URLS);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "namespace" in the asset statement. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNoNamespace() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements =
                String.format("[{\"relation\": [\"%s\"], \"target\": {\"site\": \"%s\"}}]",
                        RELATION_HANDLE_ALL_URLS, ORIGIN);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app is related, but not to the web namespace. */
    @Test
    @Feature({"InstalledApp"})
    public void testNonWebAssetStatement() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(PACKAGE_NAME_1, "play", RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has no "site" in the asset statement. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementNoSite() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String statements =
                String.format("[{\"relation\": [\"%s\"], \"target\": {\"namespace\": \"%s\"}}]",
                        RELATION_HANDLE_ALL_URLS, NAMESPACE_WEB);
        setStringResource(PACKAGE_NAME_1, ASSET_STATEMENTS_KEY, statements);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has a syntax error in the "site" field of the asset statement. */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementSiteSyntaxError() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_SYNTAX_ERROR);

        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Android app has a "site" field missing certain parts of the URI (scheme, host, port). */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementSiteMissingParts() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_SCHEME);
        RelatedApplication[] expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_HOST);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);

        setAssetStatement(
                PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN_MISSING_PORT);
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is related with a path part in the "site" field.
     *
     * <p>The path part shouldn't really be there (according to the Digital Asset Links spec), but
     * if it is, we are lenient and just ignore it (matching only the origin).
     */
    @Test
    @Feature({"InstalledApp"})
    public void testAssetStatementSiteHasPath() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        String site = ORIGIN + "/path";
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, site);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * One related Android app; Android app is installed and mutually related.
     *
     * <p>Another Android app relates to the web app, but not mutual.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testExtraInstalledApp() {
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /**
     * Two related Android apps; Android apps both installed and mutually related.
     *
     * <p>Web app also related to an app with the same name on another platform, and another Android
     * app which is not installed.
     */
    @Test
    @Feature({"InstalledApp"})
    public void testMultipleInstalledRelatedApps() {
        RelatedApplication[] manifestRelatedApps = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null),
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_2, null),
                createRelatedApplication(PLATFORM_OTHER, PACKAGE_NAME_2, null),
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_3, null)};

        setAssetStatement(PACKAGE_NAME_2, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        setAssetStatement(PACKAGE_NAME_3, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        RelatedApplication[] expectedInstalledRelatedApps =
                new RelatedApplication[] {manifestRelatedApps[1], manifestRelatedApps[3]};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }

    /** Tests the pseudo-random artificial delay to counter a timing attack. */
    @Test
    @Feature({"InstalledApp"})
    public void testArtificialDelay() {
        byte[] salt = {0x64, 0x09, -0x68, -0x25, 0x70, 0x11, 0x25, 0x24, 0x68, -0x1a, 0x08, 0x79,
                -0x12, -0x50, 0x3b, -0x57, -0x17, -0x4d, 0x46, 0x02};
        PackageHash.setGlobalSaltForTesting(salt);
        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);

        // Installed app.
        RelatedApplication manifestRelatedApps[] = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null)};
        RelatedApplication[] expectedInstalledRelatedApps = manifestRelatedApps;
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, packageName encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(2, mInstalledAppProvider.getLastDelayMillis());

        // Non-installed app.
        manifestRelatedApps = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_2, null)};
        expectedInstalledRelatedApps = new RelatedApplication[] {};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
        // This expectation is based on HMAC_SHA256(salt, packageName encoded in UTF-8), taking the
        // low 10 bits of the first two bytes of the result / 100.
        Assert.assertEquals(5, mInstalledAppProvider.getLastDelayMillis());
    }

    @Test
    @Feature({"InstalledApp"})
    public void testMultipleAppsIncludingInstantApps() {
        RelatedApplication[] manifestRelatedApps = new RelatedApplication[] {
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_1, null),
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_2, null),
                createRelatedApplication(PLATFORM_OTHER, PACKAGE_NAME_2, null),
                createRelatedApplication(PLATFORM_ANDROID, PACKAGE_NAME_3, null),

                // Instant Apps:
                createRelatedApplication(
                        PLATFORM_ANDROID, InstalledAppProviderImpl.INSTANT_APP_ID_STRING, ORIGIN),
                createRelatedApplication(PLATFORM_ANDROID,
                        InstalledAppProviderImpl.INSTANT_APP_HOLDBACK_ID_STRING, ORIGIN)};

        setAssetStatement(PACKAGE_NAME_1, NAMESPACE_WEB, RELATION_HANDLE_ALL_URLS, ORIGIN);
        mFakeInstantAppsHandler.addInstantApp(ORIGIN, true);

        RelatedApplication[] expectedInstalledRelatedApps =
                new RelatedApplication[] {manifestRelatedApps[0], manifestRelatedApps[5]};
        verifyInstalledApps(manifestRelatedApps, expectedInstalledRelatedApps);
    }
}
