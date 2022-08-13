//package com.ark.browser.tab;
//
//import android.util.SparseArray;
//
//import org.chromium.ui.base.WindowAndroid;
//
//public class TabControlSizeManager {
//
//    private static final SparseArray<ControlSize> controlSizeArray = new SparseArray<>();
//
//    private static class ControlSize {
//        private int mTopControlsHeight;
//        private int mBottomControlsHeight;
//        private boolean mControlsResizeView;
//
//        public void setTopControlsHeight(WindowAndroid windowAndroid, int height, boolean controlsResizeView) {
//            float scale = windowAndroid.getDisplay().getDipScale();
//            mTopControlsHeight = (int) (height / scale);
//            mControlsResizeView = controlsResizeView;
//        }
//
//        public void setBottomControlsHeight(WindowAndroid windowAndroid, int height) {
//            float scale = windowAndroid.getDisplay().getDipScale();
//            mBottomControlsHeight = (int) (height / scale);
//        }
//
//        int getTopControlsHeight() {
//            return mTopControlsHeight;
//        }
//
//        int getBottomControlsHeight() {
//            return mBottomControlsHeight;
//        }
//
//        boolean controlsResizeView() {
//            return mControlsResizeView;
//        }
//
//    }
//
//    public static int getTopControlsHeight(int pageId) {
//
//        ControlSize controlSize = controlSizeArray.get(pageId);
//        if (controlSize == null) {
//            return 0;
//        }
//
//        return controlSize.mTopControlsHeight;
//    }
//
//    int getBottomControlsHeight(int pageId) {
//        ControlSize controlSize = controlSizeArray.get(pageId);
//        if (controlSize == null) {
//            return 0;
//        }
//
//        return controlSize.mBottomControlsHeight;
//    }
//
//    boolean controlsResizeView(int pageId) {
//        ControlSize controlSize = controlSizeArray.get(pageId);
//        if (controlSize == null) {
//            return false;
//        }
//
//        return controlSize.mControlsResizeView;
//    }
//
//}
