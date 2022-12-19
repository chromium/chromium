package com.ark.browser.ui.fragment.dialog;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.ark.browser.core.utils.TabPrinter;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageCacheManager;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.ui.widget.DialogHeaderLayout;
import com.zpj.fragmentation.dialog.base.OverDragBottomDialogFragment;
import com.zpj.toast.ZToast;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

public class ShareDialog extends OverDragBottomDialogFragment<ShareDialog> implements View.OnClickListener {


    public static void start(Context context) {
        ShareDialog mainMenuPopup = new ShareDialog();
        mainMenuPopup.show(context);
    }

    @Override
    protected int getImplLayoutId() {
        return R.layout.fragment_dialog_share;
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {
        super.initView(view, savedInstanceState);
        TextView share_link = findViewById(R.id.share_link);
        TextView share_screenshot = findViewById(R.id.share_screenshot);
        TextView share_pdf = findViewById(R.id.share_pdf);
        TextView share_open_by_other = findViewById(R.id.share_open_by_other);

        DialogHeaderLayout headerLayout = findViewById(R.id.layout_dialog_header);
        headerLayout.setOnCloseClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                dismiss();
            }
        });

        share_link.setOnClickListener(this);
        share_screenshot.setOnClickListener(this);
        share_pdf.setOnClickListener(this);
        share_open_by_other.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        ArkTabImpl tab = (ArkTabImpl) TabListManager.getInstance().getCurrentNativeTab();
        if (tab == null) {
            return;
        }

        int id = v.getId();
        if (id == R.id.share_link) {
            share(tab);
        } else if (id == R.id.share_screenshot) {
//                ScreenshotSource mScreenshotTask = new ScreenshotTask(_mActivity);
//                mScreenshotTask.capture(new Runnable() {
//                    @Override
//                    public void run() {
//                        ZToast.normal("截图成功 TODO分享");
////                                mActivity.setWallpaper(new BitmapDrawable(mActivity.getResources(), mScreenshotTask.getScreenshot()));
//                    }
//                });
            ZToast.warning("TODO截图分享");
        } else if (id == R.id.share_pdf) {
            PrintingController printingController = PrintingControllerImpl.getInstance();
            if (printingController != null && !printingController.isBusy()) {
                Log.d("print_id", "print_id");
                printingController.startPrint(new TabPrinter(tab),
                        new PrintManagerDelegateImpl(_mActivity));
                RecordUserAction.record("MobileMenuPrint");
            }
        } else if (id == R.id.share_open_by_other) {
            if (OfflinePageUtils.isOfflinePage(tab)) {
                share(tab);
                return;
            }
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setData(Uri.parse(tab.getPageInfo().getUrl()));
            Intent chooser = Intent.createChooser(intent, "其它应用打开链接");
            context.startActivity(chooser);
        }
        dismiss();
    }

    private void share(Tab tab) {
//        ShareMenuActionHandler.getInstance().onShareMenuItemSelected(
//                _mActivity, tab, false, TabListManager.getInstance().isIncognitoSelected());
    }

}

