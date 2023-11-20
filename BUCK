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