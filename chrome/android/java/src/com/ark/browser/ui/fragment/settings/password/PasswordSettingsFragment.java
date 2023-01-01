package com.ark.browser.ui.fragment.settings.password;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageButton;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.ui.fragment.base.BaseSwipeBackFragment;
import com.ark.browser.utils.KeyguardUtil;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.toast.ZToast;
import com.zpj.widget.setting.CheckableSettingItem;
import com.zpj.widget.setting.CommonSettingItem;
import com.zpj.widget.setting.OnCheckableItemClickListener;
import com.zpj.widget.setting.OnCommonItemClickListener;
import com.zpj.widget.setting.SwitchSettingItem;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.password_manager.settings.PasswordUIView;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * The "Save passwords" screen in Settings, which allows the user to enable or disable password
 * saving, to view saved passwords (just the username and URL), and to delete saved passwords.
 */
public class PasswordSettingsFragment
        extends BaseSwipeBackFragment
        implements PasswordUIView.Observer {
    // Keys for name/password dictionaries.
    public static final String PASSWORD_LIST_URL = "url";
    public static final String PASSWORD_LIST_NAME = "name";
    public static final String PASSWORD_LIST_PASSWORD = "password";

    // Used to pass the password id into a new activity.
    public static final String PASSWORD_LIST_ID = "id";

    // The key for saving |mExportRequested| to instance bundle.
    private static final String SAVED_STATE_EXPORT_REQUESTED = "saved-state-export-requested";

    public static final String PREF_SAVE_PASSWORDS_SWITCH = "save_passwords_switch";
    public static final String PREF_AUTOSIGNIN_SWITCH = "autosignin_switch";

    private boolean mNoPasswords;
    private boolean mNoPasswordExceptions;
    // True if the user triggered the password export flow and this fragment is waiting for the
    // result of the user's reauthentication.
    private boolean mExportRequested;

    private PasswordUIView mPasswordUIView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mPasswordUIView = new PasswordUIView();
        mPasswordUIView.setObserver(this);

        if (ReauthenticationManager.isReauthenticationApiAvailable()) {
            setHasOptionsMenu(true);
        }
        if (savedInstanceState != null
                && savedInstanceState.containsKey(SAVED_STATE_EXPORT_REQUESTED)) {
            mExportRequested =
                    savedInstanceState.getBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
        }
    }

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_setting_save_password;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        setToolbarTitle("密码管理");

        SwitchSettingItem savedPasswordsItem = view.findViewById(R.id.item_saved_passwords);

        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());

        savedPasswordsItem.setChecked(prefService.getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE));
        savedPasswordsItem.setOnItemClickListener(new OnCheckableItemClickListener() {
            @Override
            public void onItemClick(CheckableSettingItem item) {
                prefService.setBoolean(Pref.CREDENTIALS_ENABLE_SERVICE, item.isChecked());
            }
        });

        SwitchSettingItem autoSignInItem = view.findViewById(R.id.item_auto_sign_in);
        autoSignInItem.setChecked(prefService.getBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN));
        autoSignInItem.setOnItemClickListener(new OnCheckableItemClickListener() {
            @Override
            public void onItemClick(CheckableSettingItem item) {
                prefService.setBoolean(Pref.CREDENTIALS_ENABLE_AUTOSIGNIN, item.isChecked());
            }
        });

        CommonSettingItem item1 = view.findViewById(R.id.item_1);
        item1.setOnItemClickListener(new OnCommonItemClickListener() {
            @Override
            public void onItemClick(CommonSettingItem item) {

            }
        });
    }

    @Override
    public void toolbarRightImageButton(@NonNull ImageButton imageButton) {
        super.toolbarRightImageButton(imageButton);
        imageButton.setOnClickListener(v -> {
            if (!ReauthenticationManager.isScreenLockSetUp(getActivity().getApplicationContext())) {
                ZToast.warning("需在“设置”部分中开启屏幕锁定功能，才能从此设备中导出您的密码");
            } else if (ReauthenticationManager.authenticationStillValid()) {
                exportAfterReauth();
            } else {
                mExportRequested = true;
//                    start(PasswordReauthenticationFragment.newInstance());
                KeyguardUtil.with(getActivity()).lockDevice();

//                ReauthenticationManager.displayReauthenticationFragment(
//                        R.string.lockscreen_description_export, getView().getId(),
//                        getFragmentManager());
            }
        });
    }

    /** Continues with the password export flow after the user successfully reauthenticated. */
    private void exportAfterReauth() {
        ZDialog.alert()
                .setTitle("Warning")
                .setContent("所有能查看此导出文件的人员都能看到您的密码。")
                .setPositiveButton((fragment, which) -> {
                    exportAfterWarning();
                })
                .show(context);
//        ZPopup.alert(getContext())
//                .setTitle("Warning")
//                .setContent(getString(R.string.settings_passwords_export_description))
//                .setConfirmButton(new OnConfirmListener<AlertPopup>() {
//                    @Override
//                    public void onConfirm(AlertPopup popup) {
//                        exportAfterWarning();
//                    }
//                })
//                .show();
//        ExportWarningDialogFragment exportWarningDialogFragment = new ExportWarningDialogFragment();
//        exportWarningDialogFragment.setExportWarningHandler(new DialogInterface.OnClickListener() {
//            /** On positive button response asks the parent to continue with the export flow. */
//            @Override
//            public void onClick(DialogInterface dialog, int which) {
//                if (which == AlertDialog.BUTTON_POSITIVE) {
//
//                }
//            }
//        });
//        exportWarningDialogFragment.show(getChildFragmentManager(), null);
    }

    private void exportAfterWarning() {
        // TODO(crbug.com/788701): Start the export.
    }

    /**
     * Empty screen message when no passwords or exceptions are stored.
     */
    private void displayEmptyScreenMessage() {
    }

    @Override
    public void onDetach() {
        super.onDetach();
        ReauthenticationManager.resetLastReauth();
    }

    void rebuildPasswordLists() {
        mNoPasswords = false;
        mNoPasswordExceptions = false;
        createSavePasswordsSwitch();
        createAutoSignInCheckbox();
        mPasswordUIView.updatePasswordLists();
    }

    @Override
    public void passwordListAvailable(int count) {

        mNoPasswords = count == 0;
        if (mNoPasswords) {
            if (mNoPasswordExceptions) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        for (int i = 0; i < count; i++) {
            mPasswordUIView.getSavedPasswordEntry(i);
        }
    }

    @Override
    public void passwordExceptionListAvailable(int count) {

        mNoPasswordExceptions = count == 0;
        if (mNoPasswordExceptions) {
            if (mNoPasswords) displayEmptyScreenMessage();
            return;
        }

        displayManageAccountLink();

        for (int i = 0; i < count; i++) {
            mPasswordUIView.getSavedPasswordException(i);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mExportRequested) {
            mExportRequested = false;
            if (ReauthenticationManager.authenticationStillValid()) exportAfterReauth();
        }
        rebuildPasswordLists();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(SAVED_STATE_EXPORT_REQUESTED, mExportRequested);
    }

    @Override
    public void onDestroyView() {
        mPasswordUIView.destroy();
        mPasswordUIView = null;
        super.onDestroyView();
    }

    private void createSavePasswordsSwitch() {

    }

    private void createAutoSignInCheckbox() {

    }

    private void displayManageAccountLink() {

    }

}

