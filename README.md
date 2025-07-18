# ![Chromium Logo](chrome/app/theme/chromium/product_logo_64.png) Chromium

Chromium is an open-source browser project focused on building a safer, faster, and more stable web experience for everyone.

## 🌐 Project Website  
[https://www.chromium.org](https://www.chromium.org)

## 📦 Getting the Source Code  
> **Do not** use `git clone` directly.  
Instead, follow the official instructions here:  
[docs/get_the_code.md](docs/get_the_code.md)

## 📚 Documentation  
Primary project documentation is available at:  
[docs/README.md](docs/README.md)

To understand the layout of the codebase, refer to:  
[Getting Around the Chromium Source Code Directory Structure](https://www.chromium.org/developers/how-tos/getting-around-the-chrome-source-code)

## 🗂️ Directory Structure  
Due to historical reasons, a few small top-level directories still exist.  
The current guideline is to use top-level directories only for major products (e.g., `Chrome`, `Android WebView`, `Ash`).  
Even if a product includes multiple executables, its code should remain within its own subdirectory.

## 🐞 Found a Bug?  
Please file an issue here:  
[https://crbug.com/new](https://crbug.com/new)
