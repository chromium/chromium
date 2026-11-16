// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.content.Context;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.components.browser_ui.widget.containment.ContainmentItem;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.google_apis.gaia.CoreAccountId;

/** A dedicated preference for the account settings top avatar. */
@NullMarked
public class CentralAccountCardPreference extends Preference
        implements ProfileDataCache.Observer, ContainmentItem {
    private DisplayableProfileData mPrimaryAccount;
    private ProfileDataCache mProfileDataCache;
    private final boolean mAiTierRingEnabled;

    public CentralAccountCardPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // TODO(crbug.com/576944682): Keep only the ring layout after flag cleanup.
        mAiTierRingEnabled = SigninFeatureMap.getInstance().isAiTierRingInSettingsEnabled();
        setLayoutResource(
                mAiTierRingEnabled
                        ? R.layout.central_account_card_view
                        : R.layout.central_account_card_view_legacy);
    }

    @Override
    public @BackgroundStyle int getCustomBackgroundStyle() {
        return BackgroundStyle.NONE;
    }

    /**
     * Initialize the dependencies for the CentralAccountCardPreference.
     *
     * <p>Must be called before the preference is attached, which is called from the containing
     * settings screen's onViewCreated method.
     */
    @Initializer
    public void initialize(CoreAccountId primaryAccountId, ProfileDataCache profileDataCache) {
        mPrimaryAccount = profileDataCache.getById(primaryAccountId);
        mProfileDataCache = profileDataCache;
    }

    @Override
    public void onAttached() {
        super.onAttached();

        mProfileDataCache.addObserver(this);
    }

    @Override
    public void onDetached() {
        super.onDetached();

        mProfileDataCache.removeObserver(this);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ImageView imageView = (ImageView) holder.findViewById(R.id.central_account_image);
        imageView.setImageDrawable(mPrimaryAccount.getImage());

        Pair<String, String> primaryAndSecondaryText = getPrimaryAndSecondaryText(mPrimaryAccount);

        TextView primaryText = (TextView) holder.findViewById(R.id.central_account_primary_text);
        primaryText.setText(primaryAndSecondaryText.first);

        TextView secondaryText =
                (TextView) holder.findViewById(R.id.central_account_secondary_text);
        if (!primaryAndSecondaryText.second.isEmpty()) {
            secondaryText.setText(primaryAndSecondaryText.second);
            secondaryText.setVisibility(View.VISIBLE);
        } else {
            secondaryText.setVisibility(View.GONE);
        }

        // Only the ring layout has the AI tier pill.
        if (mAiTierRingEnabled) {
            bindAiTierPill(holder, primaryAndSecondaryText);
        }
    }

    /** ProfileDataCache.Observer implementation. */
    @Override
    public void onProfileDataUpdated(DisplayableProfileData profileData) {
        if (!profileData.getAccountId().equals(mPrimaryAccount.getAccountId())) {
            return;
        }
        mPrimaryAccount = profileData;
        notifyChanged();
    }

    /**
     * Shows the AI tier pill below the avatar if the account has a tier name, and adds the
     * membership to the card's description while the account has the AI tier ring. The pill itself
     * is hidden from screen readers and isn't interactive.
     */
    private void bindAiTierPill(
            PreferenceViewHolder holder, Pair<String, String> primaryAndSecondaryText) {
        TextView pill = (TextView) holder.findViewById(R.id.central_account_ai_tier_pill);
        String pillName = mPrimaryAccount.getAiTierPillName();
        final boolean showsPill = !pillName.isEmpty();
        if (showsPill) {
            pill.setText(pillName);
            // Each view gets its own background, since a drawable can't be shared by views shown at
            // the same time (e.g. the old and new views of a change animation). Rebinding the same
            // view keeps it, so it's only redrawn when its size changes.
            if (pill.getBackground() == null) {
                pill.setBackground(mProfileDataCache.createAiTierPillDrawable());
            }
        }
        pill.setVisibility(showsPill ? View.VISIBLE : View.GONE);

        // Like the New Tab Page's identity disc, screen readers hear about the membership whenever
        // the ring is shown, even without a pill (e.g. for tiers without a name).
        String description =
                mPrimaryAccount.hasAiTierRing()
                        ? getDescriptionWithMembership(
                                primaryAndSecondaryText,
                                showsPill ? mPrimaryAccount.getAiTierFullName() : "")
                        : null;

        // As for TabCardLabelView, the texts aren't read separately when the card has a
        // description: the card is then one stop for screen readers, without keyboard focus.
        View card = holder.findViewById(R.id.central_account_card);
        card.setContentDescription(description);
        card.setScreenReaderFocusable(description != null);
        int textImportance =
                description == null
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                        : View.IMPORTANT_FOR_ACCESSIBILITY_NO;
        holder.findViewById(R.id.central_account_primary_text)
                .setImportantForAccessibility(textImportance);
        holder.findViewById(R.id.central_account_secondary_text)
                .setImportantForAccessibility(textImportance);
    }

    /**
     * Returns the card's description with the membership.
     *
     * @param primaryAndSecondaryText The texts shown on the card.
     * @param tierFullName The tier's full name, or empty if it isn't known (e.g. the pill isn't
     *     shown, or the server overrides it with an empty name). The description then only says
     *     "membership", with "Google" in branded builds, like the New Tab Page's identity disc.
     */
    private String getDescriptionWithMembership(
            Pair<String, String> primaryAndSecondaryText, String tierFullName) {
        Context context = getContext();
        boolean singleLine = primaryAndSecondaryText.second.isEmpty();
        if (tierFullName.isEmpty()) {
            return singleLine
                    ? context.getString(
                            R.string.account_settings_card_description_single_line_with_membership,
                            primaryAndSecondaryText.first)
                    : context.getString(
                            R.string.account_settings_card_description_with_membership,
                            primaryAndSecondaryText.first,
                            primaryAndSecondaryText.second);
        }
        return singleLine
                ? context.getString(
                        R.string.account_settings_card_description_single_line_with_tier,
                        primaryAndSecondaryText.first,
                        tierFullName)
                : context.getString(
                        R.string.account_settings_card_description_with_tier,
                        primaryAndSecondaryText.first,
                        primaryAndSecondaryText.second,
                        tierFullName);
    }

    private Pair<String, String> getPrimaryAndSecondaryText(DisplayableProfileData profileData) {
        if (!TextUtils.isEmpty(profileData.getFullName())
                && profileData.hasDisplayableEmailAddress()) {
            return new Pair<>(profileData.getFullName(), profileData.getAccountEmail());
        } else if (!TextUtils.isEmpty(profileData.getFullName())) {
            return new Pair<>(profileData.getFullName(), "");
        } else if (profileData.hasDisplayableEmailAddress()) {
            return new Pair<>(profileData.getAccountEmail(), "");
        }
        // When email and full name cannot be shown, use the default account string instead.
        return new Pair<>(getContext().getString(R.string.default_google_account_username), "");
    }

    /** Returns the profile data shown by the card. */
    DisplayableProfileData getProfileDataForTesting() {
        return mPrimaryAccount;
    }
}
