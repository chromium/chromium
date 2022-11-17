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
* The smaller images should be in BMP (uncompressed) format.
* Each of the smaller images (48 and less) should have an 8-bit and 32-bit
  version.
* The 256 image should not be last (there is a bug in Gnome on Linux where icons
  look corrupted if the PNG image is last).

If you are creating an ICO from a set of PNGs of different sizes, the following
process (using ImageMagick and GIMP) satisfies the above conditions:

1. Convert each of the smaller images to 8-bit. With ImageMagick:

    ```sh
    for f in FILENAME-??.png; \
        do convert $f -dither None -colors 256 \
            png8:`basename $f .png`-indexed.png; \
    done
    ```

2. Combine the images into an ICO file. With ImageMagick:

    ```sh
    convert FILENAME-256.png FILENAME-{48,32,16}{-indexed,}.png FILENAME.ico
    ```

3. Unfortunately, the 8-bit images have been converted back into 32-bit images.
   Open the icon in GIMP and re-export it. This will also convert the large
   256 image into a compressed PNG.

4. Run `src/tools/resources/optimize-ico-files.py` on the resulting .ico file.

You can also run `src/tools/resources/optimize-ico-files.py` on existing .ico
files. This will convert BMPs to PNGs and run a basic PNG optimization pass, as
well as fix up any broken image masks (http://crbug.com/534679).
