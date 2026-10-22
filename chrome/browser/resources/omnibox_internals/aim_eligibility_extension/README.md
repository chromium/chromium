Instructions to locally test the AIM Eligibility component extension via the Component Updater.

## 1. Enable the Feature

Enable `chrome://flags/#aim-eligibility-component-extension`.

> **Important:** The flag must be enabled before deploying files; otherwise, the startup cleanup task will remove unrecognized component directories on launch.

## 2. Make Frontend Changes & Build

Make frontend code changes in `chrome/browser/resources/omnibox_internals/aim_eligibility/`.

Build the extension resources zip archive:
```bash
autoninja -C out/Default chrome/browser/resources/omnibox_internals/aim_eligibility_extension:build_zip
```

## 3. Deploy to User Data Directory

Locate the User Data Directory for your platform and browser:

| Platform | Chromium | Google Chrome (Official Build) |
| :--- | :--- | :--- |
| **macOS** | `$HOME/Library/Application Support/Chromium` | `$HOME/Library/Application Support/Google/Chrome` |
| **Linux** | `$HOME/.config/chromium` | `$HOME/.config/google-chrome` |
| **Windows** | `%LOCALAPPDATA%\Chromium\User Data` | `%LOCALAPPDATA%\Google\Chrome\User Data` |

Unpack `resources.zip` into the component directory, rename the extension manifest to `extension_manifest.json` with a bumped version (to avoid conflicting with Omaha's component manifest), and create the component `manifest.json`:

```bash
USER_DATA_DIR="<user-data-dir-from-table-above>"
VERSION="1.1"
COMPONENT_DIR="$USER_DATA_DIR/aim_eligibility_extension/$VERSION"

mkdir -p "$COMPONENT_DIR"
unzip -o out/Default/gen/chrome/browser/resources/omnibox_internals/aim_eligibility_extension/resources.zip \
  -d "$COMPONENT_DIR/"
sed "s/\"version\": \"1.0\"/\"version\": \"$VERSION\"/" \
  "$COMPONENT_DIR/manifest.json" > "$COMPONENT_DIR/extension_manifest.json"
cat << EOF > "$COMPONENT_DIR/manifest.json"
{
  "manifest_version": 2,
  "name": "AIM Eligibility Component Extension",
  "version": "$VERSION"
}
EOF
```

> **Note:** `ComponentLoader` validates that a candidate extension's version in `extension_manifest.json` is strictly greater than the bundled version (`1.0`).

## 4. Verify Component Installation

1. Launch Chrome and navigate to `chrome://components/`.
2. Verify that **AIM Eligibility Component Extension** is listed with **Version**: `1.1` (matching `$VERSION`).
3. Restart Chrome.
   > **Note:** The component updater stages newly discovered extension manifests into `Local State` preferences on first run. On subsequent startup, `ComponentLoader` loads the staged extension from disk.
4. Open `chrome://aim` and verify your changes are loaded dynamically from the versioned component folder.

## 5. Uninstall the Component

To cleanly uninstall the component and reset preferences to load the bundled extension:

1. Launch Chrome with `--enable-component-uninstall`.
2. Navigate to `chrome://components/`.
3. Locate **AIM Eligibility Component Extension** and click **Uninstall** next to "Check for update".
   > **Note:** This invokes `ComponentUpdateService::UnregisterComponent`, deleting the installed files from disk and triggering `OnCustomUninstall()` to clear the staged extension preferences from `Local State`.

## 6. Verify Component Uninstallation

1. Restart Chrome and navigate to `chrome://components/`.
2. Verify that **AIM Eligibility Component Extension** is listed with **Version**: `0.0.0.0`.
3. Open `chrome://aim` and verify it falls back to loading the bundled component extension.
