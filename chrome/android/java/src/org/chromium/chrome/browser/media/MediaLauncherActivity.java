// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.webkit.MimeTypeMap;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.util.IntentUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/**
 * The MediaLauncherActivity handles media-viewing Intents from other apps. It takes the given
 * content:// URI from the Intent and properly routes it to a media-viewing CustomTabActivity.
 */
public class MediaLauncherActivity extends Activity {
    // UMA histogram values for media types the user can open.
    // Keep in sync with MediaLauncherActivityMediaType enum in enums.xml.
    @IntDef({MediaType.AUDIO, MediaType.IMAGE, MediaType.VIDEO})
    @Retention(RetentionPolicy.SOURCE)
    @interface MediaType {
        int AUDIO = 0;
        int IMAGE = 1;
        int VIDEO = 2;
        int UNKNOWN = 3;
        int NUM_ENTRIES = 4;
    }

    private static CachedMetrics.EnumeratedHistogramSample sMediaTypeHistogram =
            new CachedMetrics.EnumeratedHistogramSample(
                    "MediaLauncherActivity.MediaType", MediaType.NUM_ENTRIES);

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Intent input = IntentUtils.sanitizeIntent(getIntent());
        Uri contentUri = input.getData();
        String mimeType = getMIMEType(contentUri);
        int mediaType = MediaViewerUtils.getMediaTypeFromMIMEType(mimeType);

        sMediaTypeHistogram.record(mediaType);

        if (mediaType == MediaType.UNKNOWN) {
            // With our intent-filter, we should only receive implicit intents with media MIME
            // types. If we receive a non-media MIME type, it is likely a malicious explicit intent,
            // so we should not proceed.
            finish();
            return;
        }

        // TODO(https://crbug.com/800880): Determine file:// URI when possible.
        Intent intent = MediaViewerUtils.getMediaViewerIntent(
                contentUri, contentUri, mimeType, false /* allowExternalAppHandlers */);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE,
                CustomTabIntentDataProvider.LaunchSourceType.MEDIA_LAUNCHER_ACTIVITY);
        startActivity(intent);

        finish();
    }

    private String getMIMEType(Uri uri) {
        // With a content URI, we can just query the ContentResolver.
        if (uri.getScheme().equals(ContentResolver.SCHEME_CONTENT)) {
            return getContentResolver().getType(uri);
        }

        // Otherwise, use the file extension.
        String filteredUri = filterURI(uri);
        String fileExtension = MimeTypeMap.getFileExtensionFromUrl(filteredUri);
        return MimeTypeMap.getSingleton().getMimeTypeFromExtension(
                fileExtension.toLowerCase(Locale.ROOT));
    }

    // MimeTypeMap.getFileExtensionFromUrl fails when the file name includes certain special
    // characters, so we filter those out of the URI when determining the MIME type.
    protected static String filterURI(Uri uri) {
        String uriString = uri.toString();
        int filterIndex = uriString.length();

        int fragmentIndex = uriString.lastIndexOf('#', filterIndex);
        if (fragmentIndex >= 0) filterIndex = fragmentIndex;

        int queryIndex = uriString.lastIndexOf('?', filterIndex);
        if (queryIndex >= 0) filterIndex = queryIndex;

        int extensionIndex = uriString.lastIndexOf('.', filterIndex);
        if (extensionIndex >= 0) filterIndex = extensionIndex;

        return uriString.substring(0, filterIndex).replaceAll("['$!]", "")
                + uriString.substring(filterIndex);
    }
}
