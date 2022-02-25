// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Browser;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Bottom sheet content to display a list of devices a user can send a tab to after they have
 * chosen to share it with themselves through the SendTabToSelfFeature. If the user is signed-out
 * or no target devices are available, a prompt will be shown indicating to the user that
 * they must sign in to use the feature.
 */
public class DevicePickerBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private final Context mContext;
    private final BottomSheetController mController;
    private ViewGroup mToolbarView;
    private ViewGroup mContentView;
    private final DevicePickerBottomSheetAdapter mAdapter;
    private final Profile mProfile;
    private final String mUrl;
    private final String mTitle;
    private final long mNavigationTime;

    private static final int ACCOUNT_AVATAR_SIZE_DP = 24;

    public DevicePickerBottomSheetContent(Context context, String url, String title,
            long navigationTime, BottomSheetController controller) {
        mContext = context;
        mController = controller;
        mProfile = Profile.getLastUsedRegularProfile();
        mAdapter = new DevicePickerBottomSheetAdapter(mProfile);
        mUrl = url;
        mTitle = title;
        mNavigationTime = navigationTime;

        createToolbarView();
        createContentView();
    }

    private void createToolbarView() {
        mToolbarView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_device_picker_toolbar, null);
        TextView toolbarText = mToolbarView.findViewById(R.id.device_picker_toolbar);
        toolbarText.setText(R.string.send_tab_to_self_sheet_toolbar);
    }

    private void createContentView() {
        List<TargetDeviceInfo> targetDeviceList = new ArrayList<TargetDeviceInfo>();
        SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(mProfile, targetDeviceList);

        // First check if sharing is unavailable, e.g. because there are no target devices. If so,
        // show |sharingUnavailableView|, modulo adjusting the strings and the visibility of the
        // settings button.
        ViewGroup sharingUnavailableView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_feature_unavailable_prompt, null);
        TextView title = sharingUnavailableView.findViewById(R.id.title);
        TextView instructionsToEnable =
                sharingUnavailableView.findViewById(R.id.instructions_to_enable);
        if (targetDeviceList.isEmpty()) {
            mContentView = sharingUnavailableView;
            title.setText(R.string.send_tab_to_self_share_activity_title);
            instructionsToEnable.setText(R.string.send_tab_to_self_when_signed_in_unavailable);
            mToolbarView.setVisibility(View.GONE);
            // TODO(crbug.com/1298185): This is cumulating both signed-out and single device users.
            // Those should be recorded separately instead.
            RecordUserAction.record("SharingHubAndroid.SendTabToSelf.NoTargetDevices");
            return;
        }

        // Sharing is available.
        mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_device_picker_list, null);
        ListView listView = mContentView.findViewById(R.id.device_picker_list);
        listView.setAdapter(mAdapter);
        listView.setOnItemClickListener(this);

        createManageDevicesLink(listView);
    }

    private void createManageDevicesLink(ListView deviceListView) {
        ViewGroup containerView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_manage_devices_link, null);
        deviceListView.addFooterView(containerView);

        AccountInfo account = getSharingAccountInfo();
        assert account != null : "The user must be signed in to share a tab";

        // The avatar can be null in tests.
        if (account.getAccountImage() != null) {
            RoundedCornerImageView avatarView = containerView.findViewById(R.id.account_avatar);
            int accountAvatarSizePx = Math.round(
                    ACCOUNT_AVATAR_SIZE_DP * mContext.getResources().getDisplayMetrics().density);
            avatarView.setImageBitmap(Bitmap.createScaledBitmap(
                    account.getAccountImage(), accountAvatarSizePx, accountAvatarSizePx, false));
            avatarView.setRoundedCorners(accountAvatarSizePx / 2, accountAvatarSizePx / 2,
                    accountAvatarSizePx / 2, accountAvatarSizePx / 2);
        }

        Resources resources = mContext.getResources();
        // The link is opened in a new tab to avoid exiting the current page, which the user
        // possibly wants to share (maybe they just clicked "Manage devices" by mistake).
        SpannableString linkText = SpanApplier.applySpans(
                resources.getString(
                        R.string.send_tab_to_self_manage_devices_link, account.getEmail()),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(
                                mContext, this::openManageDevicesPageInNewTab)));
        TextView linkView = containerView.findViewById(R.id.manage_devices_link);
        linkView.setText(linkText);
        linkView.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private void openManageDevicesPageInNewTab(View unused) {
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(UrlConstants.GOOGLE_ACCOUNT_DEVICE_ACTIVITY_URL))
                        .setClass(mContext, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        mContext.startActivity(intent);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Return DISABLED to ensure that the entire bottom sheet is shown.
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // Return WRAP_CONTENT to have the bottom sheet only open as far as it needs to display the
        // list of devices and nothing beyond that.
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        MetricsRecorder.recordDeviceClickedInShareSheet();
        TargetDeviceInfo targetDeviceInfo = mAdapter.getItem(position);

        SendTabToSelfAndroidBridge.addEntry(
                mProfile, mUrl, mTitle, mNavigationTime, targetDeviceInfo.cacheGuid);

        Resources res = mContext.getResources();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_V2)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.UPCOMING_SHARING_FEATURES)) {
            String deviceType = res.getString(R.string.send_tab_to_self_device_type_generic);
            if (targetDeviceInfo.deviceType == TargetDeviceInfo.DeviceType.PHONE) {
                deviceType = res.getString(R.string.send_tab_to_self_device_type_phone);
            } else if (targetDeviceInfo.deviceType == TargetDeviceInfo.DeviceType.WIN
                    || targetDeviceInfo.deviceType == TargetDeviceInfo.DeviceType.MACOSX
                    || targetDeviceInfo.deviceType == TargetDeviceInfo.DeviceType.LINUX
                    || targetDeviceInfo.deviceType == TargetDeviceInfo.DeviceType.CHROMEOS) {
                deviceType = res.getString(R.string.send_tab_to_self_device_type_computer);
            }

            String toastMessage = res.getString(R.string.send_tab_to_self_v2_toast, deviceType);
            Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT).show();
        } else {
            String toastMessage =
                    res.getString(R.string.send_tab_to_self_toast, targetDeviceInfo.deviceName);
            Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT).show();
        }

        mController.hideContent(this, true);
    }

    private static AccountInfo getSharingAccountInfo() {
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        return identityManager.findExtendedAccountInfoByEmailAddress(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN).getEmail());
    }
}
