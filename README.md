# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project that is aimed at building a safer, faster, and more stable way for all users to experience the web. The project is hosted on the website [https://www.chromium.org](https://www.chromium.org).

## Getting Started
To check out the source code locally, it is recommended to not use the `git clone` command. Instead, follow the [instructions on how to get the code](https://github.com/chromium/chromium/blob/main/docs/get_the_code.md) available on the Chromium website.

## Directory Structure
The documentation for the Chromium project is rooted in [docs/README.md](docs/README.md). It is recommended to read the documentation to get familiar with the project. [Learn how to Get Around](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code) the Chromium Source Code Directory Structure.

For historical reasons, there are some small top-level directories. The guidance now is that new top-level directories are created for the product (e.g., Chrome, Android WebView, Ash). Even if these products have multiple executables, the code should be in subdirectories of the product.

## Reporting Issues
If you encounter any bugs or issues while using Chromium, please report them at [https://crbug.com/new](https://crbug.com/new).
