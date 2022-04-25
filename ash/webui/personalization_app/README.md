# ChromeOS Personalization Hub

## Environment Setup
### VSCode

- Follow [vscode setup](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/vscode.md).
- Create `tsconfig.json` using [helper script](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/webui/personalization_app/tools/gen_tsconfig.py).
  Please follow the help doc in the header of the helper script.
- Edit `${PATH_TO_CHROMIUM}/src/.git/info/exclude` and add these lines
  ```
  /ash/webui/personalization_app/resources/tsconfig.json
  /chrome/test/data/webui/chromeos/personalization_app/tsconfig.json
  ```
