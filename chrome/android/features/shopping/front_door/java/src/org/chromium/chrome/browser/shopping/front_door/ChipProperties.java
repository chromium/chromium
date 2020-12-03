package org.chromium.chrome.browser.shopping.front_door;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

public class ChipProperties {
    public static String EDIT_CHIP_ID = "EDIT_CHIP";
    public interface ToggleHandler {
        void run(String id, boolean isCategory, boolean isChecked);
    }
    public static WritableObjectPropertyKey<String> ID = new WritableObjectPropertyKey<>();
    public static WritableBooleanPropertyKey IS_CATEGORY_CHIP = new WritableBooleanPropertyKey();
    public static WritableObjectPropertyKey<String> TEXT = new WritableObjectPropertyKey<>();
    public static WritableIntPropertyKey ICON_RESOURCE_ID = new WritableIntPropertyKey();
    public static WritableBooleanPropertyKey IS_CHECKED = new WritableBooleanPropertyKey();
    public static WritableObjectPropertyKey<ToggleHandler> TOGGLE_ACTION_HANDLER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            ID, IS_CATEGORY_CHIP, TEXT, ICON_RESOURCE_ID, IS_CHECKED, TOGGLE_ACTION_HANDLER};
}
