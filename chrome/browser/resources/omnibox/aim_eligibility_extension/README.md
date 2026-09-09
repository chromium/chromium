Instructions to locally test the AIM Eligibility component extension via the Component Updater on macOS.

## 1. Enable the Feature

Enable `chrome://flags/#aim-eligibility-component-extension` or launch Chrome with: `--enable-features=AimEligibilityComponentExtension`.

> **Important:** The flag must be enabled before deploying files; otherwise, startup cleanup task will remove unrecognized component directories on launch.

## 2. Make Frontend Changes & Build

Make frontend code changes in `chrome/browser/resources/omnibox/aim_eligibility/`.

> **Note:** Any files included in `build_webui("build")` will automatically be included in `resources.grd` and unpacked into the output folder.

Build and unpack the extension resources to `out/Default/aim_eligibility_extension/`:
```bash
autoninja -C out/Default chrome/browser/resources/omnibox/aim_eligibility_extension:unpack_extension
```

## 3. Deploy to User Data Directory

Copy the unpacked extension into the component directory:

**Chromium:**

```bash
mkdir -p ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0
cp -r out/Default/aim_eligibility_extension/. ~/Library/Application\ Support/Chromium/aim_eligibility_extension/1.0/
```

**Google Chrome (Official Build):**

```bash
mkdir -p ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0
cp -r out/Default/aim_eligibility_extension/. ~/Library/Application\ Support/Google/Chrome/aim_eligibility_extension/1.0/
```

## 4. Verify Component Installation

1. Launch Chrome and navigate to:
   ```text
   chrome://components/
   ```
2. Verify that **AIM Eligibility Component Extension** is listed with:
   * **Version**: `1.0` (matching the directory)
   * **Status**: `Up-to-date` or `Component not updated`
3. Open the extension page:
   ```text
   chrome-extension://kgjeljgkbckpoekmgjfplammhcggiiaf/aim_eligibility.html
   ```
4. Verify your changes are loaded dynamically from the versioned component folder.
