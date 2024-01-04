// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export default `#version 100
precision mediump float;
precision highp int;

uniform vec4 u_top_left_background_color;
uniform vec4 u_bottom_right_background_color;
uniform vec4 u_sparkle_color;
uniform vec2 u_resolution;
uniform float u_time;
uniform float u_pixel_density;

uniform float u_grid_num;
uniform float u_luma_matte_blend_factor;
uniform float u_luma_matte_overall_brightness;
uniform float u_inverse_luma;
uniform float u_opacity;
uniform vec3 u_noise_move;

const float PI = 3.1415926535897932384626;
const float PI_ROTATE_RIGHT = PI * 0.0078125;
const float PI_ROTATE_LEFT = PI * -0.0078125;
const float ONE_THIRD = 1. / 3.;

vec4 get_uv_and_density_adjusted_uv(vec2 frag_coord,
                                    vec2 resolution_pixels,
                                    float pixel_density_divisor,
                                    float aspect) {
  vec2 uv = frag_coord / resolution_pixels;
  vec2 density_adjusted_uv =
      uv - mod(uv, pixel_density_divisor / resolution_pixels);

  density_adjusted_uv.y = (1.0 - density_adjusted_uv.y) * aspect;
  uv.y = (1.0 - uv.y) * aspect;

  return vec4(uv, density_adjusted_uv);
}

float get_loudness(vec4 opposing_color) {
  return 0.45 +
         (2. * opposing_color.r + opposing_color.g + 0.5 * opposing_color.b) *
             ONE_THIRD * 0.55;
}

highp float triangle_noise(highp vec2 n) {
  n = fract(n * vec2(5.3987, 5.4421));
  n += dot(n.yx, n.xy + vec2(21.5351, 14.3137));
  float xy = n.x * n.y;
  // compute in [0..2[ and remap to [-1.0..1.0[
  return fract(xy * 95.4307) + fract(xy * 75.04961) - 1.0;
}

highp float sparkles(highp vec2 uv, highp float t) {
  highp float n = triangle_noise(uv);
  highp float s = 0.0;
  for (highp float i = 0.0; i < 4.0; i += 1.0) {
    highp float l = i * 0.01;
    highp float h = l + 0.1;
    highp float o = smoothstep(n - l, h, n);
    o *= abs(sin(PI * o * (t + 0.55 * i)));
    s += o;
  }
  return s;
}

vec2 circular_offset(float time, vec2 dist) {
  float r = time * PI;
  return dist * vec2(cos(r), sin(r));
}

mat2 rotate2d(float rad) {
  return mat2(cos(rad), -sin(rad), sin(rad), cos(rad));
}

float soft_circle(vec2 uv, vec2 xy, float radius, float blur) {
  float blur_half = blur / 2.0;
  float d = distance(uv, xy);
  return 1.0 - smoothstep(radius - blur_half, radius + blur_half, d);
}

float circle_grid(vec2 resolution,
                  vec2 coord,
                  float time,
                  vec2 center,
                  float rotation,
                  float cell_diameter) {
  coord =
      rotate2d(rotation + cell_diameter * 10. * PI) * (center - coord) + center;
  coord = mod(coord, cell_diameter) / resolution;

  float normal_radius = cell_diameter / resolution.y * 0.5;
  float radius = 0.65 * normal_radius;

  return soft_circle(coord, vec2(normal_radius), radius, radius);
}

float turbulence(vec2 uv, float t) {
  vec2 scale = vec2(1.5);

  t = t * 10.0;

  vec2 o1 = scale * 0.5 + circular_offset(t * 0.01, scale * vec2(0.55));
  vec2 o2 = scale * 0.2 + circular_offset(t / -150.0, scale * vec2(0.45));
  vec2 o3 = scale + circular_offset(t / 300.0, scale * vec2(0.35));

  uv = uv * scale;

  float g1 = circle_grid(scale, uv, t, o1, t * PI_ROTATE_RIGHT, 0.17);
  float g2 = circle_grid(scale, uv, t, o2, t * PI_ROTATE_LEFT, 0.2);
  float g3 = circle_grid(scale, uv, t, o3, t * PI_ROTATE_RIGHT, 0.275);

  return smoothstep(0., 1., (g1 + g2 + g3) * 0.625);
}

vec2 distort(vec2 p,
             float time,
             float distort_amount_radial,
             float distort_amount_xy) {
  float angle = atan(p.y, p.x);
  return p +
         vec2(sin(angle * 8.0 + time * 0.003 + 1.641),
              cos(angle * 5.0 + 2.14 + time * 0.00412)) *
             distort_amount_radial +
         vec2(sin(p.x * 0.01 + time * 0.00215 + 0.8123),
              cos(p.y * 0.01 + time * 0.005931)) *
             distort_amount_xy;
}

// Perceived luminosity, not absolute luminosity.
float get_luminosity(vec3 c) {
  return 0.3 * c.r + 0.59 * c.g + 0.11 * c.b;
}

// Creates a luminosity mask and clamp to the legal range.
vec3 mask_luminosity(vec3 dest, float lum) {
  dest.rgb *= vec3(lum);
  // Clip back into the legal range
  dest = clamp(dest, vec3(0.), vec3(1.0));
  return dest;
}

// Return range [-1, 1].
vec3 hash(vec3 p) {
  p = fract(p * vec3(.3456, .1234, .9876));
  p += dot(p, p.yxz + 43.21);
  p = (p.xxy + p.yxx) * p.zyx;
  return (fract(sin(p) * 4567.1234567) - .5) * 2.;
}

// Skew factors (non-uniform).
const float SKEW = 0.3333333;    // 1/3
const float UNSKEW = 0.1666667;  // 1/6

// Return range roughly [-1,1].
// It's because the hash function (that returns a random gradient vector)
// returns different magnitude of vectors. Noise doesn't have to be in the
// precise range thus skipped normalize.
float simplex3d(vec3 p) {
  // Skew the input coordinate, so that we get squashed cubical grid
  vec3 s = floor(p + (p.x + p.y + p.z) * SKEW);

  // Unskew back
  vec3 u = s - (s.x + s.y + s.z) * UNSKEW;

  // Unskewed coordinate that is relative to p, to compute the noise
  // contribution based on the distance.
  vec3 c0 = p - u;

  // We have six simplices (in this case tetrahedron, since we are in 3D) that
  // we could possibly in. Here, we are finding the correct tetrahedron (simplex
  // shape), and traverse its four vertices (c0..3) when computing noise
  // contribution. The way we find them is by comparing c0's x,y,z values. For
  // example in 2D, we can find the triangle (simplex shape in 2D) that we are
  // in by comparing x and y values. i.e. x>y lower, x<y, upper triangle. Same
  // applies in 3D.
  //
  // Below indicates the offsets (or offset directions) when c0=(x0,y0,z0)
  // x0>y0>z0: (1,0,0), (1,1,0), (1,1,1)
  // x0>z0>y0: (1,0,0), (1,0,1), (1,1,1)
  // z0>x0>y0: (0,0,1), (1,0,1), (1,1,1)
  // z0>y0>x0: (0,0,1), (0,1,1), (1,1,1)
  // y0>z0>x0: (0,1,0), (0,1,1), (1,1,1)
  // y0>x0>z0: (0,1,0), (1,1,0), (1,1,1)
  //
  // The rule is:
  // * For offset1, set 1 at the max component, otherwise 0.
  // * For offset2, set 0 at the min component, otherwise 1.
  // * For offset3, set 1 for all.
  //
  // Encode x0-y0, y0-z0, z0-x0 in a vec3
  vec3 en = c0 - c0.yzx;
  // Each represents whether x0>y0, y0>z0, z0>x0
  en = step(vec3(0.), en);
  // en.zxy encodes z0>x0, x0>y0, y0>x0
  vec3 offset1 = en * (1. - en.zxy);       // find max
  vec3 offset2 = 1. - en.zxy * (1. - en);  // 1-(find min)
  vec3 offset3 = vec3(1.);

  vec3 c1 = c0 - offset1 + UNSKEW;
  vec3 c2 = c0 - offset2 + UNSKEW * 2.;
  vec3 c3 = c0 - offset3 + UNSKEW * 3.;

  // Kernel summation: dot(max(0, r^2-d^2))^4, noise contribution)
  //
  // First compute d^2, squared distance to the point.
  vec4 w;  // w = max(0, r^2 - d^2))
  w.x = dot(c0, c0);
  w.y = dot(c1, c1);
  w.z = dot(c2, c2);
  w.w = dot(c3, c3);

  // Noise contribution should decay to zero before they cross the simplex
  // boundary. Usually r^2 is 0.5 or 0.6; 0.5 ensures continuity but 0.6
  // increases the visual quality for the application where discontinuity isn't
  // noticeable.
  w = max(0.6 - w, 0.);

  // Noise contribution from each point.
  vec4 nc;
  nc.x = dot(hash(s), c0);
  nc.y = dot(hash(s + offset1), c1);
  nc.z = dot(hash(s + offset2), c2);
  nc.w = dot(hash(s + offset3), c3);

  nc *= w * w * w * w;

  // Add all the noise contributions.
  // Should multiply by the possible max contribution to adjust the range in
  // [-1,1].
  return dot(vec4(32.), nc);
}

// Random rotations.
// The way you create fractal noise is layering simplex noise with some
// rotation. To make random cloud looking noise, the rotations should not align.
// (Otherwise it creates patterned noise). Below rotations only rotate in one
// axis.
const mat3 rot1 = mat3(1.0, 0., 0., 0., 0.15, -0.98, 0., 0.98, 0.15);
const mat3 rot2 = mat3(-0.95, 0., -0.3, 0., 1., 0., 0.3, 0., -0.95);
const mat3 rot3 = mat3(1.0, 0., 0., 0., -0.44, -0.89, 0., 0.89, -0.44);

// Octave = 4
// Divide each coefficient by 3 to produce more grainy noise.
float simplex3d_fractal(vec3 p) {
  return 0.675 * simplex3d(p * rot1) + 0.225 * simplex3d(2.0 * p * rot2) +
         0.075 * simplex3d(4.0 * p * rot3) + 0.025 * simplex3d(8.0 * p);
}

float saturate(float value) {
  return clamp(value, 0.0, 1.0);
}

// Screen blend
vec3 screen(vec3 dest, vec3 src) {
  return dest + src - dest * src;
}

void main() {
  float aspect = u_resolution.y / u_resolution.x;
  vec4 compound_uv = get_uv_and_density_adjusted_uv(
      gl_FragCoord.xy, u_resolution, u_pixel_density, aspect);
  vec2 uv = compound_uv.xy;
  vec2 density_adjusted_uv = compound_uv.zw;

  vec2 gradient_direction = gl_FragCoord.xy / u_resolution;
  float gradient =
      saturate((1.0 - gradient_direction.y) * aspect + gradient_direction.x);
  vec4 background_color = mix(u_top_left_background_color,
                              u_bottom_right_background_color, gradient);

  // apply noise
  vec3 noise_p =
      vec3(density_adjusted_uv + u_noise_move.xy, u_noise_move.z) * u_grid_num;
  vec3 color = background_color.rgb;

  // Add dither with triangle distribution to avoid color banding. Dither in the
  // shader here as we are in gamma space.
  float dither = triangle_noise(gl_FragCoord.xy * u_pixel_density) / 255.;

  // The result color should be pre-multiplied, i.e. [R*A, G*A, B*A, A], thus
  // need to multiply rgb with a to get the correct result.
  color = (color + dither) * u_opacity;
  vec4 noise = vec4(color, u_opacity);

  float sparkle_luma = 1.0 - get_luminosity(vec3(simplex3d(noise_p)));
  sparkle_luma = max(/* intensity= */ 1.75 * sparkle_luma - /* dim= */ 1.3, 0.);
  float sparkle_alpha =
      sparkles(gl_FragCoord.xy - mod(gl_FragCoord.xy, u_pixel_density * 0.8),
               u_time / 1000.0);
  vec4 sparkle =
      vec4(mask_luminosity(u_sparkle_color.rgb * sparkle_alpha, sparkle_luma) *
               u_sparkle_color.a,
           u_sparkle_color.a);

  vec3 effect = noise.rgb + sparkle.rgb;
  gl_FragColor =
      mix(background_color, vec4(effect, 1.0), smoothstep(0., 0.75, 0.3));
}`;
