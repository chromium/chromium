# ChromeOS Personalization Hub

## Environment Setup
### VSCode

- Follow https://chromium.googlesource.com/chromium/src/+/HEAD/docs/vscode.md
- Config `tsconfig.json`:
  - Create or update `${PATH_TO_CHROMIUM}/src/ash/webui/personalization_app/resources/tsconfig.json`
    - Remember to replace `out/${YOUR_BUILD}` with your out directory.
  ```
  {
    "__comment__": [
        "This file is used by local typescript language server. It is manually",
        "maintained to be close to the corresponding ts_library() target in BUILD.gn. ",
        "Be sure to replace locally 'out/${YOUR_BUILD}' with your out dir"
    ],
    "extends": "./tsconfig_base.json",
    "compilerOptions": {
        "composite": true,
        "rootDirs": [
            ".",
            "../../../../out/${YOUR_BUILD}/gen/ash/webui/personalization_app/resources/preprocessed"
        ],
        "moduleResolution": "node",
        "noEmit": true,
        "paths": {
            "/*": [
                "./*"
            ],
            "chrome://resources/*": [
                "../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/preprocessed/*"
            ],
            "//resources/*": [
                "../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/preprocessed/*"
            ],
            "chrome://resources/polymer/v3_0/*": [
                "../../../../third_party/polymer/v3_0/components-chromium/*"
            ],
            "//resources/polymer/v3_0/*": [
                "../../../../third_party/polymer/v3_0/components-chromium/*"
            ],
            "chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js": [
                "../../../../third_party/polymer/v3_0/components-chromium/polymer/polymer.d.ts"
            ],
            "//resources/polymer/v3_0/polymer/polymer_bundled.min.js": [
                "../../../../third_party/polymer/v3_0/components-chromium/polymer/polymer.d.ts"
            ],
            "/tools/typescript/definitions/*": [
                "../../../../tools/typescript/definitions/*"
            ]
        }
    },
    "references": [
        {
            "path": "../../../../out/${YOUR_BUILD}/gen/third_party/polymer/v3_0/tsconfig.json"
        },
        {
            "path": "../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/tsconfig.json"
        },
        {
            "path": "../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/mojo/tsconfig.json"
        }
    ]
  }
  ```
  - Create or update `${PATH_TO_CHROMIUM}/src/chrome/test/data/webui/chromeos/personalization_app/tsconfig.json`
  ```
  {
    "extends": "./tsconfig_base.json",
    "compilerOptions": {
        "rootDirs": [
            ".",
            "../../../../../../out/${YOUR_BUILD}/gen/chrome/test/data/webui/chromeos/personalization_app"
        ],
        "paths": {
            "chrome://resources/*": [
                "../../../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/preprocessed/*"
            ],
            "//resources/*": [
                "../../../../../../out/${YOUR_BUILD}/gen/ui/webui/resources/preprocessed/*"
            ],
            "chrome://resources/polymer/v3_0/*": [
                "../../../../../../third_party/polymer/v3_0/components-chromium/*"
            ],
            "//resources/polymer/v3_0/*": [
                "../../../../../../third_party/polymer/v3_0/components-chromium/*"
            ],
            "chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js": [
                "../../../../../../third_party/polymer/v3_0/components-chromium/polymer/polymer.d.ts"
            ],
            "//resources/polymer/v3_0/polymer/polymer_bundled.min.js": [
                "../../../../../../third_party/polymer/v3_0/components-chromium/polymer/polymer.d.ts"
            ],
            "/tools/typescript/definitions/*": [
                "../../../../../../tools/typescript/definitions/*"
            ],
            "chrome://personalization/*": [
                "../../../../../../out/${YOUR_BUILD}/gen/ash/webui/personalization_app/resources/tsc/*"
            ],
            "chrome://webui-test/*": [
                "../../../../../../out/${YOUR_BUILD}/gen/chrome/test/data/webui/tsc/*"
            ]
        }
    },
    "include": [
        "*.ts"
    ],
    "references": [
        {
            "path": "../../../../../../ash/webui/personalization_app/resources/tsconfig.json"
        }
    ]
  }
  ```
  - Edit `${PATH_TO_CHROMIUM}/src/.git/info/exclude` and add these lines
  ```
  /ash/webui/personalization_app/resources/tsconfig.json
  /chrome/test/data/webui/chromeos/personalization_app/tsconfig.json
  ```
