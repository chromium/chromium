// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.net.Uri;

import androidx.browser.trusted.sharing.ShareData;
import androidx.browser.trusted.sharing.ShareTarget;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.WebApkShareTarget;

import java.util.ArrayList;
import java.util.List;

/** Tests WebApkShareTargetUtil. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {WebApkShareTargetUtilTest.WebApkShareTargetUtilShadow.class})
public class WebApkShareTargetUtilTest {
    /** Builder class for {@link WebApkShareTarget} */
    public static class ShareTargetBuilder {
        private String mAction;
        private @ShareTarget.RequestMethod String mMethod;
        private @ShareTarget.EncodingType String mEncodingType;
        private String mParamTitle;
        private String mParamText;
        private List<String> mParamFileNames = new ArrayList<>();
        private List<String[]> mParamFileAccepts = new ArrayList<>();

        public ShareTargetBuilder(String action) {
            mAction = action;
        }

        public void setMethod(@ShareTarget.RequestMethod String method) {
            mMethod = method;
        }

        public void setEncodingType(@ShareTarget.EncodingType String encodingType) {
            mEncodingType = encodingType;
        }

        public void setParamTitle(String paramTitle) {
            mParamTitle = paramTitle;
        }

        public void setParamText(String paramText) {
            mParamText = paramText;
        }

        public void addParamFile(String name, String[] accepts) {
            mParamFileNames.add(name);
            mParamFileAccepts.add(accepts);
        }

        public void setParamFiles(List<String> names, List<String[]> accepts) {
            mParamFileNames = names;
            mParamFileAccepts = accepts;
        }

        WebApkShareTarget build() {
            String[] paramFileNames = null;
            if (mParamFileNames != null) {
                paramFileNames = mParamFileNames.toArray(new String[0]);
            }
            String[][] paramFileAccepts = null;
            if (mParamFileAccepts != null) {
                paramFileAccepts = mParamFileAccepts.toArray(new String[0][]);
            }
            return new WebApkShareTarget(
                    mAction,
                    mParamTitle,
                    mParamText,
                    ShareTarget.METHOD_POST.equalsIgnoreCase(mMethod),
                    ShareTarget.ENCODING_TYPE_MULTIPART.equalsIgnoreCase(mEncodingType),
                    paramFileNames,
                    paramFileAccepts);
        }
    }

    private static void assertPostData(
            WebApkShareTargetUtil.PostData postData,
            String[] names,
            boolean[] isValueFileUris,
            String[] values,
            String[] fileNames,
            String[] types) {
        Assert.assertNotNull(postData);

        Assert.assertNotNull(postData.names);
        Assert.assertEquals(postData.names.size(), names.length);
        for (int i = 0; i < names.length; i++) {
            Assert.assertEquals(postData.names.get(i), names[i]);
        }

        Assert.assertNotNull(postData.isValueFileUri);
        Assert.assertEquals(postData.isValueFileUri.size(), isValueFileUris.length);
        for (int i = 0; i < isValueFileUris.length; i++) {
            Assert.assertEquals(postData.isValueFileUri.get(i), isValueFileUris[i]);
        }

        Assert.assertNotNull(postData.values);
        Assert.assertEquals(postData.values.size(), values.length);
        for (int i = 0; i < values.length; i++) {
            Assert.assertEquals(new String(postData.values.get(i)), values[i]);
        }

        Assert.assertNotNull(postData.filenames);
        Assert.assertEquals(postData.filenames.size(), fileNames.length);
        for (int i = 0; i < fileNames.length; i++) {
            Assert.assertEquals(postData.filenames.get(i), fileNames[i]);
        }

        Assert.assertNotNull(postData.types);
        Assert.assertEquals(postData.types.size(), types.length);
        for (int i = 0; i < types.length; i++) {
            Assert.assertEquals(postData.types.get(i), types[i]);
        }
    }

    /** Shadow class for {@link WebApkShareTargetUtil} which mocks out ContentProvider queries. */
    @Implements(WebApkShareTargetUtil.class)
    public static class WebApkShareTargetUtilShadow extends WebApkShareTargetUtil {
        @Implementation
        public static byte[] readStringFromContentUri(Uri uri) {
            return String.format("content-for-%s", uri.toString()).getBytes();
        }

        @Implementation
        public static String getFileTypeFromContentUri(Uri uri) {
            String uriString = uri.toString();
            if (uriString.startsWith("text")) {
                return "text/plain";
            }
            return "image/gif";
        }

        @Implementation
        public static String getFileNameFromContentUri(Uri uri) {
            return String.format("file-name-for-%s", uri.toString());
        }
    }

    /** Test that post data is null when the share method is GET. */
    @Test
    public void testGET() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_GET);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_URL_ENCODED);

        ShareData shareData = new ShareData(/* title= */ null, /* title= */ null, /* uris= */ null);

        Assert.assertEquals(null, computePostData(shareTargetBuilder.build(), shareData));

        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        Assert.assertNotNull(computePostData(shareTargetBuilder.build(), shareData));
    }

    /**
     * Test that post data for application/x-www-form-urlencoded will contain
     * the correct information in the correct place.
     */
    @Test
    public void testPostUrlEncoded() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_URL_ENCODED);
        shareTargetBuilder.setParamTitle("title");
        shareTargetBuilder.setParamText("text");

        ShareData shareData = new ShareData("extra_subject", "extra_text", /* uris= */ null);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"title", "text"},
                new boolean[] {false, false},
                new String[] {"extra_subject", "extra_text"},
                new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that
     * multipart/form-data with no names/accepts output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoNamesNoAccepts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-1"));
        ShareData shareData = new ShareData(/* title= */ null, /* text= */ null, uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {},
                new boolean[] {},
                new String[] {},
                new String[] {},
                new String[] {});
    }

    /**
     * Test that multipart/form-data with no files or text specified in Intent.EXTRA_STREAM will
     * output a null postdata.
     */
    @Test
    public void testPostMultipartWithNoFilesNorText() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});

        WebApkShareTargetUtil.PostData postData =
                computePostData(
                        shareTargetBuilder.build(),
                        new ShareData(/* title= */ null, /* text= */ null, /* uris= */ null));

        assertPostData(
                postData,
                new String[] {},
                new boolean[] {},
                new String[] {},
                new String[] {},
                new String[] {});
    }

    @Test
    public void testPostMultipartWithFiles() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-2"));
        ShareData shareData = new ShareData(/* title= */ null, /* text= */ null, uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"name"},
                new boolean[] {true},
                new String[] {"mock-uri-2"},
                new String[] {"file-name-for-mock-uri-2"},
                new String[] {"image/gif"});
    }

    @Test
    public void testPostMultipartWithTexts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});
        shareTargetBuilder.setParamText("share-text");
        shareTargetBuilder.setParamTitle("share-title");

        ShareData shareData =
                new ShareData("shared_subject_value", "shared_text_value", /* uris= */ null);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"},
                new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithTextsOnlyTarget() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.setParamText("share-text");
        shareTargetBuilder.setParamTitle("share-title");

        ShareData shareData =
                new ShareData("shared_subject_value", "shared_text_value", /* uris= */ null);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"},
                new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    @Test
    public void testPostMultipartWithFileAndTexts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});
        shareTargetBuilder.setParamText("share-text");
        shareTargetBuilder.setParamTitle("share-title");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri-3"));
        ShareData shareData = new ShareData("shared_subject_value", "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-title", "share-text", "name"},
                new boolean[] {false, false, true},
                new String[] {"shared_subject_value", "shared_text_value", "mock-uri-3"},
                new String[] {"", "", "file-name-for-mock-uri-3"},
                new String[] {"text/plain", "text/plain", "image/gif"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, but we receive a text file, and that
     * we don't receive shared text, that we send the text file as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeNotInAccepts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});
        shareTargetBuilder.setParamText("share-text");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, /* text= */ null, uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text"},
                new boolean[] {true},
                new String[] {"text-file-mock-uri"},
                new String[] {""},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, but we receive multiple text files,
     * and that we don't receive shared text, that we send only one text file as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeNotInAcceptsMultiple() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});
        shareTargetBuilder.setParamText("share-text");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        uris.add(Uri.parse("text-file-mock-uri2"));
        uris.add(Uri.parse("text-file-mock-uri3"));
        ShareData shareData = new ShareData(/* title= */ null, /* text= */ null, uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text"},
                new boolean[] {true},
                new String[] {"text-file-mock-uri"},
                new String[] {""},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS doesn't accept text, and that we DO receive shared text;
     * even though we received a text file, we should ignore it, because in the end, a web page
     * expects a single value (not an array) in the "share-text" field.
     */
    @Test
    public void testPostMultipartSharedTextFileAndSharedSelection() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("name", new String[] {"image/*"});
        shareTargetBuilder.setParamText("share-text");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-file-mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text"},
                new boolean[] {false},
                new String[] {"shared_text_value"},
                new String[] {""},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_ACCEPTS DOES accept text, we don't accidentally send the text file
     * as shared text.
     */
    @Test
    public void testPostMultipartSharedTextFileMimeTypeInAccepts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("share-text-file", new String[] {"text/*"});
        shareTargetBuilder.setParamText("share-text");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, /* text= */ null, uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text-file"},
                new boolean[] {true},
                new String[] {"text-mock-uri"},
                new String[] {"file-name-for-text-mock-uri"},
                new String[] {"text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is missing but we receive a text selection, we send it as a
     * file, along with other files.
     */
    @Test
    public void testPostMultipartSharedTextSelectionNoParamTextPlainInAccepts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("share-text-file", new String[] {"text/*"});

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text-file", "share-text-file"},
                new boolean[] {false, true},
                new String[] {"shared_text_value", "text-mock-uri"},
                new String[] {"shared.txt", "file-name-for-text-mock-uri"},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is present and  we receive a text selection, we don't
     * mistakenly send it as a file. File sharing should not be affected either.
     */
    @Test
    public void testPostMultipartSharedTextSelectionHasParamText() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("share-text-file", new String[] {"text/*"});
        shareTargetBuilder.setParamText("share-text");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("text-mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text", "share-text-file"},
                new boolean[] {false, true},
                new String[] {"shared_text_value", "text-mock-uri"},
                new String[] {"", "file-name-for-text-mock-uri"},
                new String[] {"text/plain", "text/plain"});
    }

    /**
     * Test that when SHARE_PARAM_TEXT is missing, we receive a text selection, and we can't find a
     * SHARE_PARAM_ACCEPTS that matches text (such as "text/plain" or "text/*"), we don't mistakenly
     * send the text as a file. In addition, file sharing should not be affected.
     *
     * Ideally this should never happens if the WebAPK Minting server minted WebAPK correctly.
     */
    @Test
    public void testPostMultipartSharedTextSelectionNoParamTextPlainNotInAccepts() {
        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.addParamFile("share-text-file", new String[] {"image/*"});

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri"));
        ShareData shareData = new ShareData(/* title= */ null, "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        assertPostData(
                postData,
                new String[] {"share-text-file"},
                new boolean[] {true},
                new String[] {"mock-uri"},
                new String[] {"file-name-for-mock-uri"},
                new String[] {"image/gif"});
    }

    @Test
    public void testPostMultipartWithFileAndInValidParamNames() {
        List<String[]> paramFileAccepts = new ArrayList<>();
        paramFileAccepts.add(new String[] {"image/*"});

        ShareTargetBuilder shareTargetBuilder = new ShareTargetBuilder("/share.html");
        shareTargetBuilder.setMethod(ShareTarget.METHOD_POST);
        shareTargetBuilder.setEncodingType(ShareTarget.ENCODING_TYPE_MULTIPART);
        shareTargetBuilder.setParamFiles(null, paramFileAccepts);
        shareTargetBuilder.setParamText("share-text");
        shareTargetBuilder.setParamTitle("share-title");

        ArrayList<Uri> uris = new ArrayList<>();
        uris.add(Uri.parse("mock-uri"));
        ShareData shareData = new ShareData("shared_subject_value", "shared_text_value", uris);

        WebApkShareTargetUtil.PostData postData =
                computePostData(shareTargetBuilder.build(), shareData);

        // with invalid name parameter from Android manifest, we ignore the file sharing part.
        assertPostData(
                postData,
                new String[] {"share-title", "share-text"},
                new boolean[] {false, false},
                new String[] {"shared_subject_value", "shared_text_value"},
                new String[] {"", ""},
                new String[] {"text/plain", "text/plain"});
    }

    private WebApkShareTargetUtil.PostData computePostData(
            WebApkShareTarget shareTarget, ShareData shareData) {
        return WebApkShareTargetUtil.computePostData(shareTarget, shareData);
    }
}
