// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.CREDENTIAL_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.HAS_MANUAL_CHANGE_BUTTON;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_CONFIRMATION_HANDLER;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.DELETION_ORIGIN;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_TIMESTAMP;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.COMPROMISED_CREDENTIALS_COUNT;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.LAUNCH_ACCOUNT_CHECKUP_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.RESTART_BUTTON_ACTION;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.SHOW_CHECK_SUBTITLE;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.UNKNOWN_PROGRESS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.VIEW_DIALOG_HANDLER;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.method.LinkMovementMethod;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.chrome.browser.password_check.PasswordCheckProperties.ItemType;
import org.chromium.chrome.browser.password_check.helper.PasswordCheckIconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Provides functions that map {@link PasswordCheckProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link PasswordCheckFragmentView}.
 */
class PasswordCheckViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view
     * accordingly.
     *
     * @param model       The observed {@link PropertyModel}. Its data is reflected in the view.
     * @param view        The {@link PasswordCheckFragmentView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindPasswordCheckView(
            PropertyModel model, PasswordCheckFragmentView view, PropertyKey propertyKey) {
        if (propertyKey == ITEMS) {
            view.getListView()
                    .setAdapter(
                            new RecyclerViewAdapter<>(
                                    new SimpleRecyclerViewMcp<>(
                                            model.get(ITEMS),
                                            PasswordCheckProperties::getItemType,
                                            PasswordCheckViewBinder::connectPropertyModel),
                                    PasswordCheckViewBinder::createViewHolder));
        } else if (propertyKey == DELETION_CONFIRMATION_HANDLER) {
            if (model.get(DELETION_CONFIRMATION_HANDLER) == null) return; // Initial or onDismiss.
            view.showDialogFragment(
                    new PasswordCheckDeletionDialogFragment(
                            model.get(DELETION_CONFIRMATION_HANDLER), model.get(DELETION_ORIGIN)));
        } else if (propertyKey == DELETION_ORIGIN) {
            // Binding not necessary (only used indirectly).
        } else if (propertyKey == VIEW_CREDENTIAL) {
            // Binding not necessary (only used indirectly).
        } else if (propertyKey == VIEW_DIALOG_HANDLER) {
            if (model.get(VIEW_DIALOG_HANDLER) == null) return; // Initial or onDismiss.
            view.showDialogFragment(
                    new PasswordCheckViewDialogFragment(
                            model.get(VIEW_DIALOG_HANDLER), model.get(VIEW_CREDENTIAL)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new View inside the list inside the PasswordCheckFragmentView.
     *
     * @param parent   The parent {@link ViewGroup} of the new item.
     * @param itemType The type of View to create.
     */
    private static PasswordCheckViewHolder createViewHolder(
            ViewGroup parent, @ItemType int itemType) {
        switch (itemType) {
            case ItemType.HEADER:
                return new PasswordCheckViewHolder(
                        parent,
                        R.layout.password_check_header_item,
                        PasswordCheckViewBinder::bindHeaderView);
            case ItemType.COMPROMISED_CREDENTIAL:
                return new PasswordCheckViewHolder(
                        parent,
                        R.layout.password_check_compromised_credential_item,
                        PasswordCheckViewBinder::bindCredentialView);
        }
        assert false : "Cannot create view for ItemType: " + itemType;
        return null;
    }

    /**
     * This method creates a model change processor for each recycler view item when it is created.
     *
     * @param holder A {@link PasswordCheckViewHolder} holding a view and view binder for the MCP.
     * @param item   A {@link MVCListAdapter.ListItem} holding a {@link PropertyModel} for the MCP.
     */
    private static void connectPropertyModel(
            PasswordCheckViewHolder holder, MVCListAdapter.ListItem item) {
        holder.setupModelChangeProcessor(item.model);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method might
     * be called on a recycled view with old data, so make sure to always reset unused properties to
     * default values.
     *
     * @param model       The model containing the data for the view
     * @param view        The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        CompromisedCredential credential = model.get(COMPROMISED_CREDENTIAL);
        if (propertyKey == COMPROMISED_CREDENTIAL) {
            TextView originText = view.findViewById(R.id.credential_origin);
            originText.setText(credential.getDisplayOrigin());

            TextView username = view.findViewById(R.id.compromised_username);
            username.setText(credential.getDisplayUsername());

            TextView reason = view.findViewById(R.id.compromised_reason);
            reason.setText(getReasonForCredential(credential));

            ListMenuButton more = view.findViewById(R.id.credential_menu_button);
            more.setDelegate(
                    () -> {
                        return createCredentialMenu(
                                view.getContext(),
                                model.get(COMPROMISED_CREDENTIAL),
                                model.get(CREDENTIAL_HANDLER));
                    });

            ButtonCompat button = view.findViewById(R.id.credential_change_button);
            button.setOnClickListener(
                    unusedView -> {
                        model.get(CREDENTIAL_HANDLER).onChangePasswordButtonClick(credential);
                    });
            setTintListForCompoundDrawables(
                    button.getCompoundDrawablesRelative(),
                    view.getContext(),
                    R.color.default_text_color_on_accent1_list);
        } else if (propertyKey == CREDENTIAL_HANDLER) {
            assert model.get(CREDENTIAL_HANDLER) != null;
            // Is read-only and must therefore be bound initially, so no action required.
        } else if (propertyKey == HAS_MANUAL_CHANGE_BUTTON) {
            ButtonCompat button = view.findViewById(R.id.credential_change_button);
            button.setVisibility(model.get(HAS_MANUAL_CHANGE_BUTTON) ? View.VISIBLE : View.GONE);
            TextView changeHint = view.findViewById(R.id.credential_change_hint);
            changeHint.setVisibility(
                    model.get(HAS_MANUAL_CHANGE_BUTTON) ? View.GONE : View.VISIBLE);
        } else if (propertyKey == FAVICON_OR_FALLBACK) {
            ImageView imageView = view.findViewById(R.id.credential_favicon);
            PasswordCheckIconHelper.FaviconOrFallback data = model.get(FAVICON_OR_FALLBACK);
            Resources resources = view.getResources();
            Context context = view.getContext();
            imageView.setImageDrawable(
                    FaviconUtils.getIconDrawableWithoutFilter(
                            data.mIcon,
                            data.mUrlOrAppName,
                            PasswordCheckIconHelper.getIconColor(data, context),
                            FaviconUtils.createCircularIconGenerator(context),
                            resources,
                            resources.getDimensionPixelSize(R.dimen.default_favicon_size)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private static @StringRes int getReasonForCredential(CompromisedCredential credential) {
        if (!credential.isOnlyPhished()) {
            return R.string.password_check_credential_row_reason_leaked;
        }
        if (!credential.isOnlyLeaked()) {
            return R.string.password_check_credential_row_reason_phished;
        }
        return R.string.password_check_credential_row_reason_leaked_and_phished;
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view
     * accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data needs to be reflected in the view.
     * @param view  The {@link View} of the header to update.
     * @param key   The {@link PropertyKey} which changed.
     */
    private static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        Pair<Integer, Integer> progress = model.get(CHECK_PROGRESS);
        @PasswordCheckUIStatus int status = model.get(CHECK_STATUS);
        Long checkTimestamp = model.get(CHECK_TIMESTAMP);
        Integer compromisedCredentialsCount = model.get(COMPROMISED_CREDENTIALS_COUNT);
        Runnable launchCheckupInAccount = model.get(LAUNCH_ACCOUNT_CHECKUP_ACTION);
        boolean showStatusSubtitle = model.get(SHOW_CHECK_SUBTITLE);

        if (key == CHECK_PROGRESS) {
            updateStatusText(
                    view,
                    status,
                    compromisedCredentialsCount,
                    checkTimestamp,
                    progress,
                    launchCheckupInAccount);
        } else if (key == CHECK_STATUS) {
            updateActionButton(view, status, model.get(RESTART_BUTTON_ACTION));
            updateStatusIcon(view, status, compromisedCredentialsCount);
            updateStatusIllustration(view, status, compromisedCredentialsCount);
            updateStatusText(
                    view,
                    status,
                    compromisedCredentialsCount,
                    checkTimestamp,
                    progress,
                    launchCheckupInAccount);
            updateStatusSubtitle(view, status, showStatusSubtitle, compromisedCredentialsCount);
        } else if (key == CHECK_TIMESTAMP) {
            updateStatusText(
                    view,
                    status,
                    compromisedCredentialsCount,
                    checkTimestamp,
                    progress,
                    launchCheckupInAccount);
        } else if (key == COMPROMISED_CREDENTIALS_COUNT) {
            updateStatusIcon(view, status, compromisedCredentialsCount);
            updateStatusIllustration(view, status, compromisedCredentialsCount);
            updateStatusText(
                    view,
                    status,
                    compromisedCredentialsCount,
                    checkTimestamp,
                    progress,
                    launchCheckupInAccount);
            updateStatusSubtitle(view, status, showStatusSubtitle, compromisedCredentialsCount);
        } else if (key == LAUNCH_ACCOUNT_CHECKUP_ACTION) {
            assert model.get(LAUNCH_ACCOUNT_CHECKUP_ACTION) != null
                    : "Launch checkup in account is always required.";
        } else if (key == RESTART_BUTTON_ACTION) {
            assert model.get(RESTART_BUTTON_ACTION) != null : "Restart action is always required.";
        } else if (key == SHOW_CHECK_SUBTITLE) {
            updateStatusSubtitle(view, status, showStatusSubtitle, compromisedCredentialsCount);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private PasswordCheckViewBinder() {}

    private static void updateActionButton(
            View view, @PasswordCheckUIStatus int status, Runnable startCheck) {
        ImageButton restartButton = view.findViewById(R.id.check_status_restart_button);
        LinearLayout textWrapper = view.findViewById(R.id.check_status_text_layout);
        boolean shouldBeVisible = shouldShowActionButton(status);

        LinearLayout.LayoutParams layoutParams =
                (LinearLayout.LayoutParams) textWrapper.getLayoutParams();
        layoutParams.setMarginEnd(
                shouldBeVisible
                        ? 0
                        : view.getResources()
                                .getDimensionPixelSize(R.dimen.check_status_text_margin));

        restartButton.setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);
        restartButton.setOnClickListener(shouldBeVisible ? unusedView -> startCheck.run() : null);
        restartButton.setClickable(shouldBeVisible);
    }

    private static void updateStatusIcon(
            View view, @PasswordCheckUIStatus int status, Integer compromisedCredentialsCount) {
        // TODO(crbug.com/40710602): Set default values for header properties.
        if (status == PasswordCheckUIStatus.IDLE && compromisedCredentialsCount == null) return;
        ImageView statusIcon = view.findViewById(R.id.check_status_icon);
        statusIcon.setImageResource(getIconResource(status, compromisedCredentialsCount));
        statusIcon.setVisibility(getIconVisibility(status));
        view.findViewById(R.id.check_status_progress)
                .setVisibility(getProgressBarVisibility(status));
    }

    private static boolean shouldShowActionButton(@PasswordCheckUIStatus int status) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
            case PasswordCheckUIStatus.ERROR_OFFLINE:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return true;
            case PasswordCheckUIStatus.RUNNING:
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
                return false;
        }
        assert false : "Unhandled check status " + status + "on action button update";
        return false;
    }

    private static int getIconResource(
            @PasswordCheckUIStatus int status, Integer compromisedCredentialsCount) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
                assert compromisedCredentialsCount != null;
                return compromisedCredentialsCount == 0
                        ? R.drawable.ic_check_circle_filled_green_24dp
                        : R.drawable.ic_warning_red_24dp;
            case PasswordCheckUIStatus.RUNNING:
                return 0;
            case PasswordCheckUIStatus.ERROR_OFFLINE:
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return R.drawable.ic_error_grey800_24dp_filled;
            default:
                assert false : "Unhandled check status " + status + "on icon update";
        }
        return 0;
    }

    private static int getIconVisibility(@PasswordCheckUIStatus int status) {
        return status == PasswordCheckUIStatus.RUNNING ? View.GONE : View.VISIBLE;
    }

    private static int getProgressBarVisibility(@PasswordCheckUIStatus int status) {
        return status == PasswordCheckUIStatus.RUNNING ? View.VISIBLE : View.GONE;
    }

    private static void updateStatusText(
            View view,
            @PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount,
            Long checkTimestamp,
            Pair<Integer, Integer> progress,
            Runnable launchCheckupInAccount) {
        // TODO(crbug.com/40710602): Set default values for header properties.
        if (status == PasswordCheckUIStatus.IDLE
                && (compromisedCredentialsCount == null || checkTimestamp == null)) {
            return;
        }
        if (status == PasswordCheckUIStatus.RUNNING && progress == null) return;

        TextView statusMessage = view.findViewById(R.id.check_status_message);
        statusMessage.setText(
                getStatusMessage(
                        view,
                        status,
                        compromisedCredentialsCount,
                        progress,
                        launchCheckupInAccount));
        statusMessage.setMovementMethod(LinkMovementMethod.getInstance());

        LinearLayout textLayout = view.findViewById(R.id.check_status_text_layout);
        int verticalMargin = getDimensionPixelOffset(view, getStatusTextMargin(status));
        textLayout.setPadding(0, verticalMargin, 0, verticalMargin);

        TextView statusDescription = view.findViewById(R.id.check_status_description);
        statusDescription.setText(getStatusDescription(view, checkTimestamp));
        statusDescription.setVisibility(getStatusDescriptionVisibility(status));
    }

    private static CharSequence getStatusMessage(
            View view,
            @PasswordCheckUIStatus int status,
            Integer compromisedCredentialsCount,
            Pair<Integer, Integer> progress,
            Runnable launchCheckupInAccount) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
                assert compromisedCredentialsCount != null;
                return compromisedCredentialsCount == 0
                        ? getString(view, R.string.password_check_status_message_idle_no_leaks)
                        : getResources(view)
                                .getQuantityString(
                                        R.plurals.password_check_status_message_idle_with_leaks,
                                        compromisedCredentialsCount,
                                        compromisedCredentialsCount);
            case PasswordCheckUIStatus.RUNNING:
                assert progress != null;
                if (progress.equals(UNKNOWN_PROGRESS)) {
                    return getString(view, R.string.password_check_status_message_initial_running);
                } else {
                    return String.format(
                            getString(view, R.string.password_check_status_message_running),
                            progress.first,
                            progress.second);
                }
            case PasswordCheckUIStatus.ERROR_OFFLINE:
                return getString(view, R.string.password_check_status_message_error_offline);
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
                return getString(view, R.string.password_check_status_message_error_no_passwords);
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
                return getString(view, R.string.password_check_status_message_error_signed_out);
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
                return getString(view, R.string.password_check_status_message_error_quota_limit);
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
                NoUnderlineClickableSpan linkSpan =
                        new NoUnderlineClickableSpan(
                                view.getContext(), unusedView -> launchCheckupInAccount.run());
                return SpanApplier.applySpans(
                        getString(
                                view,
                                R.string
                                        .password_check_status_message_error_quota_limit_account_check),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan));
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return getString(view, R.string.password_check_status_message_error_unknown);
            default:
                assert false : "Unhandled check status " + status + "on message update";
        }
        return null;
    }

    private static int getStatusTextMargin(@PasswordCheckUIStatus int status) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
                return R.dimen.check_status_message_idle_margin_vertical;
            case PasswordCheckUIStatus.RUNNING:
                return R.dimen.check_status_message_running_margin_vertical;
            case PasswordCheckUIStatus.ERROR_OFFLINE:
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return R.dimen.check_status_message_error_margin_vertical;
            default:
                assert false : "Unhandled check status " + status + "on text margin update";
        }
        return 0;
    }

    private static String getStatusDescription(View view, Long checkTimestamp) {
        if (checkTimestamp == null) return null;
        Resources res = getResources(view);
        return res.getString(
                R.string.password_check_status_description_idle,
                getTimestamp(res, System.currentTimeMillis() - checkTimestamp));
    }

    @VisibleForTesting
    protected static String getTimestamp(Resources res, long timeDeltaMs) {
        if (timeDeltaMs < 0) timeDeltaMs = 0;

        int daysElapsed = (int) (timeDeltaMs / (24L * 60L * 60L * 1000L));
        int hoursElapsed = (int) (timeDeltaMs / (60L * 60L * 1000L));
        int minutesElapsed = (int) (timeDeltaMs / (60L * 1000L));

        String relativeTime;
        if (daysElapsed > 0L) {
            relativeTime = res.getQuantityString(R.plurals.n_days_ago, daysElapsed, daysElapsed);
        } else if (hoursElapsed > 0L) {
            relativeTime = res.getQuantityString(R.plurals.n_hours_ago, hoursElapsed, hoursElapsed);
        } else if (minutesElapsed > 0L) {
            relativeTime =
                    res.getQuantityString(R.plurals.n_minutes_ago, minutesElapsed, minutesElapsed);
        } else {
            relativeTime = res.getString(R.string.password_check_just_now);
        }
        return relativeTime;
    }

    private static int getStatusDescriptionVisibility(@PasswordCheckUIStatus int status) {
        return status == PasswordCheckUIStatus.IDLE ? View.VISIBLE : View.GONE;
    }

    private static void updateStatusIllustration(
            View view, @PasswordCheckUIStatus int status, Integer compromisedCredentialsCount) {
        // TODO(crbug.com/40710602): Set default values for header properties.
        if (status == PasswordCheckUIStatus.IDLE && compromisedCredentialsCount == null) return;
        ImageView statusIllustration = view.findViewById(R.id.check_status_illustration);
        statusIllustration.setImageResource(
                getIllustrationResource(status, compromisedCredentialsCount));
    }

    private static int getIllustrationResource(
            @PasswordCheckUIStatus int status, Integer compromisedCredentialsCount) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
                assert compromisedCredentialsCount != null;
                return compromisedCredentialsCount == 0
                        ? R.drawable.password_check_positive
                        : R.drawable.password_checkup_warning;
            case PasswordCheckUIStatus.RUNNING:
            case PasswordCheckUIStatus.ERROR_OFFLINE:
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return R.drawable.password_check_neutral;
            default:
                assert false : "Unhandled check status " + status + "on illustration update";
        }
        return 0;
    }

    private static void updateStatusSubtitle(
            View view,
            @PasswordCheckUIStatus int status,
            boolean showStatusSubtitle,
            Integer compromisedCredentialsCount) {
        // TODO(crbug.com/40710602): Set default values for header properties.
        if (status == PasswordCheckUIStatus.IDLE && compromisedCredentialsCount == null) return;
        TextView statusSubtitle = view.findViewById(R.id.check_status_subtitle);
        statusSubtitle.setText(getSubtitleText(view, status, compromisedCredentialsCount));
        statusSubtitle.setVisibility(showStatusSubtitle ? View.VISIBLE : View.GONE);
    }

    private static String getSubtitleText(
            View view, @PasswordCheckUIStatus int status, Integer compromisedCredentialsCount) {
        switch (status) {
            case PasswordCheckUIStatus.IDLE:
                assert compromisedCredentialsCount != null;
                return compromisedCredentialsCount == 0
                        ? getString(view, R.string.password_check_status_subtitle_no_findings)
                        : getString(
                                view,
                                R.string
                                        .password_check_status_subtitle_found_compromised_credentials);
            case PasswordCheckUIStatus.RUNNING:
            case PasswordCheckUIStatus.ERROR_OFFLINE:
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return getString(
                        view,
                        R.string.password_check_status_subtitle_found_compromised_credentials);
            default:
                assert false : "Unhandled check status " + status + "on icon update";
        }
        return null;
    }

    private static ListMenu createCredentialMenu(
            Context context,
            CompromisedCredential credential,
            PasswordCheckCoordinator.CredentialEventHandler credentialHandler) {
        MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
        menuItems.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.password_check_credential_menu_item_view_button_caption,
                        0,
                        0,
                        true));
        menuItems.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.password_check_credential_menu_item_edit_button_caption,
                        0,
                        0,
                        true));
        menuItems.add(
                BrowserUiListMenuUtils.buildMenuListItem(
                        R.string.password_check_credential_menu_item_remove_button_caption,
                        0,
                        0,
                        true));
        ListMenu.Delegate delegate =
                (listModel) -> {
                    int textId = listModel.get(ListMenuItemProperties.TITLE_ID);
                    if (textId
                            == R.string.password_check_credential_menu_item_view_button_caption) {
                        credentialHandler.onView(credential);
                    } else if (textId
                            == R.string.password_check_credential_menu_item_edit_button_caption) {
                        credentialHandler.onEdit(credential, context);
                    } else if (textId
                            == R.string.password_check_credential_menu_item_remove_button_caption) {
                        credentialHandler.onRemove(credential);
                    } else {
                        assert false : "No action defined for " + context.getString(textId);
                    }
                };
        return BrowserUiListMenuUtils.getBasicListMenu(context, menuItems, delegate);
    }

    private static String getString(View view, int resourceId) {
        return getResources(view).getString(resourceId);
    }

    private static int getDimensionPixelOffset(View view, int resourceId) {
        return getResources(view).getDimensionPixelOffset(resourceId);
    }

    private static Resources getResources(View view) {
        return view.getContext().getResources();
    }

    private static void setTintListForCompoundDrawables(
            Drawable[] compoundDrawables, Context context, @ColorRes int tintColorList) {
        for (Drawable drawable : compoundDrawables) {
            if (drawable == null) continue;
            DrawableCompat.setTintList(
                    drawable, AppCompatResources.getColorStateList(context, tintColorList));
        }
    }
}
