// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.DialogTitle;

import org.chromium.chrome.browser.supervised_user.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.ElidedUrlTextView;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.url.GURL;

/** Bottom sheet content for the screen which allows a parent to approve or deny a website. */
class WebsiteApprovalSheetContent implements BottomSheetContent {
    private static final String ELLIPSIS = "...";
    static final int MAX_HOST_SIZE = 256;
    static final int SUBSTRING_LIMIT = 256;
    static final int MAX_FULL_URL_SIZE = MAX_HOST_SIZE + SUBSTRING_LIMIT;

    private final Context mContext;
    private final View mContentView;

    static final class StringSpecs {
        String mFormattedString;
        int mVisibleUrlLength;

        StringSpecs(String formattedString, int length) {
            mFormattedString = formattedString;
            mVisibleUrlLength = length;
        }
    }

    public WebsiteApprovalSheetContent(Context context) {
        mContext = context;
        mContentView =
                (LinearLayout)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.website_approval_bottom_sheet, null);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // We don't want the sheet to be too easily dismissed, as getting here requires a full
        // manual password entry.
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.parent_website_approval_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.parent_website_approval_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.parent_website_approval_closed;
    }

    public ButtonCompat getApproveButton() {
        return mContentView.findViewById(R.id.approve_button);
    }

    public ButtonCompat getDenyButton() {
        return mContentView.findViewById(R.id.deny_button);
    }

    public void setTitle(String childName) {
        DialogTitle titleView = mContentView.findViewById(R.id.website_approval_sheet_title);
        String displayedTitle =
                mContext.getString(R.string.parent_website_approval_title, childName);
        titleView.setText(displayedTitle);
        // Set for accessibility announcement.
        titleView.setContentDescription(displayedTitle);
    }

    public void setDomainText(GURL url) {
        TextView domainTextView = mContentView.findViewById(R.id.all_pages_of);
        // Omit scheme, credentials, path and trivial subdomains
        String formattedDomain =
                UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(url);
        String displayedFormattedDomain =
                mContext.getString(R.string.parent_website_approval_all_of_domain, formattedDomain);
        domainTextView.setText(displayedFormattedDomain);
        // Set for accessibility announcement.
        domainTextView.setContentDescription(displayedFormattedDomain);
    }

    @VisibleForTesting
    static StringSpecs truncateLongUrl(GURL url) {
        // Omit user-specific and trivial url parts.
        String formattedUrl =
                UrlFormatter.formatUrlForDisplayOmitSchemeOmitTrivialSubdomains(url.getSpec());

        if (formattedUrl.length() <= MAX_HOST_SIZE) {
            // Display the full url.
            return new StringSpecs(formattedUrl, formattedUrl.length());
        } else if (formattedUrl.length() <= MAX_FULL_URL_SIZE) {
            // By default display the host and url up to MAX_HOST_SIZE chars.
            // On click display the full url.
            return new StringSpecs(formattedUrl, MAX_HOST_SIZE);
        } else {
            // Omit scheme, credentials, path and trivial subdomains
            String formattedHost =
                    UrlFormatter.formatUrlForDisplayOmitSchemePathAndTrivialSubdomains(url);
            // Keep the full host but only portions of the path up to a limit.
            // The urls the we process never contain query or fragments.
            StringBuilder truncatedUrlBuilder = new StringBuilder();
            truncatedUrlBuilder.append(formattedHost);
            int truncatedPathLength = Math.min(url.getPath().length(), SUBSTRING_LIMIT);
            if (truncatedPathLength + ELLIPSIS.length() < url.getPath().length()) {
                truncatedUrlBuilder.append(url.getPath().substring(0, truncatedPathLength));
                truncatedUrlBuilder.append(ELLIPSIS);

            } else {
                truncatedUrlBuilder.append(url.getPath());
            }
            // By default display the host and url up to MAX_HOST_SIZE chars.
            // On click display the expanded url (which we may have truncated).
            return new StringSpecs(truncatedUrlBuilder.toString(), MAX_HOST_SIZE);
        }
    }

    public void setFullUrlText(GURL url) {
        ElidedUrlTextView fullUrlView = mContentView.findViewById(R.id.full_url);
        StringSpecs specs = truncateLongUrl(url);
        fullUrlView.setUrl(specs.mFormattedString, specs.mVisibleUrlLength);

        LinearLayout urlWrapper = mContentView.findViewById(R.id.url_container);
        urlWrapper.setOnClickListener(
                v -> {
                    fullUrlView.toggleTruncation();
                });

        // Set for accessibility announcement.
        fullUrlView.setContentDescription(specs.mFormattedString);
    }

    public void setFaviconBitmap(Bitmap bitmap) {
        ImageView faviconImageView = mContentView.findViewById(R.id.favicon);
        faviconImageView.setImageBitmap(bitmap);
    }
}
