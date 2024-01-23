genrule(
  name = "build-image",
  srcs = ["Dockerfile.build"],
  remote = False,
  cmd = "docker build -t chromium-build-new - < Dockerfile.build && echo chromium-build-new > $OUT",
  out = "image_name.txt",
  env = {
    "REPLAY_CHROMIUM_DOCKER_IMAGE_NAME": "chromium-build-new",
  },
  visibility = ["PUBLIC"],
)

export_file(
  name = "args.gn",
  src = select({
    "config//os:linux": "replay_build_scripts/linux_args.gn",
    "config//os:macos": select({
      "config//cpu:x86_64": "replay_build_scripts/mac_x86_64_args.gn",
      "config//cpu:arm64": "replay_build_scripts/mac_arm64_args.gn",
    }) ,
    "config//os:windows": "replay_build_scripts/windows_args.gn",
  }),
  out = "args.gn",
  visibility = ["PUBLIC"],
)