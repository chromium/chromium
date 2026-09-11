Instructions to locally test the AIM Eligibility component extension via the Component Updater on macOS.

## 1. Enable the Feature

Enable `chrome://flags/#aim-eligibility-component-extension`.

> **Important:** The flag must be enabled before deploying files; otherwise, startup cleanup task will remove unrecognized component directories on launch.

## 2. Make Frontend Changes & Build

Make frontend code changes in `chrome/browser/resources/omnibox/aim_eligibility/`.

> **Note:** Any files included in `build_webui("build")` will automatically be included in `resources.grd` and unpacked into the output folder.

Build and unpack the extension resources to `out/Default/aim_eligibility_extension/`:
```bash
autoninja -C out/Default chrome/browser/resources/omnibox/aim_eligibility_extension:unpack_extension
```

## 3. Deploy to User Data Directory

Copy the unpacked extension into the component directory, rename the extension manifest to `extension_manifest.json` (to avoid conflicting with Omaha's component manifest), and create the component `manifest.json`:

**Chromium:**

```bash
mkdir -p ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0
cp -r out/Default/aim_eligibility_extension/. ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0/
mv ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0/manifest.json \
   ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0/extension_manifest.json
cat << 'EOF' > ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0/manifest.json
{
  "manifest_version": 2,
  "name": "AIM Eligibility Component Extension",
  "version": "1.0"
}
EOF
```

**Google Chrome (Official Build):**

```bash
mkdir -p ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0
cp -r out/Default/aim_eligibility_extension/. ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0/
mv ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0/manifest.json \
   ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0/extension_manifest.json
cat << 'EOF' > ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0/manifest.json
{
  "manifest_version": 2,
  "name": "AIM Eligibility Component Extension",
  "version": "1.0"
}
EOF
```

## 4. Verify Component Installation

1. Launch Chrome and navigate to:
   ```text
   chrome://components/
   ```
2. Verify that **AIM Eligibility Component Extension** is listed with:
   * **Version**: `1.0` (matching the directory)
   * **Status**: `Up-to-date` or `Component not updated`
3. Restart Chrome.
   > **Note:** The component updater stages newly discovered extension manifests into Local State preferences on first run. On subsequent startup, `ComponentLoader` loads the staged extension from disk.
4. Open the extension page:
   ```text
   chrome-extension://kabpfonokkamggpgbldambnmkliehlbh/aim_eligibility.html
   ```
5. Verify your changes are loaded dynamically from the versioned component folder.

## 5. Uninstall the Component

To cleanly uninstall the component and reset preferences to load the bundled extension:

1. Launch Chrome with the `--enable-component-uninstall`.
2. Navigate to:
   ```text
   chrome://components/
   ```
3. Locate **AIM Eligibility Component Extension** and click the **Uninstall** button next to "Check for update".
   * This invokes `ComponentUpdateService::UnregisterComponent`, deleting the installed files from disk and triggering `OnCustomUninstall()` to clear the staged extension preferences from Local State.
4. Restart Chrome.
5. Navigate to `chrome-extension://kabpfonokkamggpgbldambnmkliehlbh/aim_eligibility.html` and verify it falls back to loading the bundled component extension.
