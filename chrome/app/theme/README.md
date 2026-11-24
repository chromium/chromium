Branded and Trademarked Assets
==============================

If the resource that you want to check in is product-branded and/or trademarked,
please read the docs on
[Google Chrome branding](../../../docs/google_chrome_branded_builds.md) to
determine the correct steps to take.

PNG Images
==========

Please run src/tools/resources/optimize-png-files.sh on all new icons. For example:

```sh
tools/resources/optimize-png-files.sh -o2 new_pngs_dir
```

If this script does not work for some reason, at least pngcrush the files:

```sh
  mkdir crushed
  pngcrush -d crushed -brute -reduce -rem alla new/*.png
```

ICO Images
==========

Windows ICO icons should be in the following format:

* A square image of each size: 256, 48, 32, 16.
* The 256 image should be in PNG format, and optimized.
* The smaller images (48 and less) should be 32-bit (ARGB) in BMP (uncompressed)
  format. 8-bit or lower bit-depth versions are not needed on Windows 10/11.

If you are creating an ICO from a set of 32-bit RGBA PNGs of different sizes,
the following process satisfies the above conditions:

1. Combine the images into an ICO file with ImageMagick:

    ```sh
    magick FILENAME-{256,48,32,16}.png FILENAME.ico
    ```

    (Use `convert` instead of `magick` on ImageMagick 6.)

2. Run `tools/resources/optimize-ico-files.py` on the resulting `.ico` file:

    ```sh
    tools/resources/optimize-ico-files.py FILENAME.ico
    ```

You can also run `tools/resources/optimize-ico-files.py` on existing `.ico`
files. This will convert large (>= 256x256) BMPs to PNGs, drop redundant
low-bit-depth (< 32-bit) BMPs when a 32-bit image of the same size is present,
run a PNG optimization pass, and fix up any broken 32-bit image masks
(https://crbug.com/41203437).
