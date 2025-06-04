#!/usr/bin/env python
import time
import tensorflow as tf

def load_mnist_data():
    (x_train, y_train), _ = tf.keras.datasets.mnist.load_data()
    # Normalize images to [0, 1] and add channel dimension.
    x_train = x_train.astype('float32') / 255.0
    x_train = tf.expand_dims(x_train, axis=-1)  # shape: (num_samples, 28, 28, 1)
    # Convert labels to one-hot encoding.
    y_train = tf.keras.utils.to_categorical(y_train, 10)
    return x_train, y_train

def build_model():
    model = tf.keras.Sequential([
        # First conv block.
        tf.keras.layers.Conv2D(32, kernel_size=3, activation='relu', input_shape=(28, 28, 1)),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.Conv2D(32, kernel_size=3, activation='relu'),
        tf.keras.layers.MaxPooling2D(pool_size=(2, 2)),
        tf.keras.layers.Dropout(0.25),
        # Second conv block.
        tf.keras.layers.Conv2D(64, kernel_size=3, activation='relu'),
        tf.keras.layers.BatchNormalization(),
        tf.keras.layers.Conv2D(64, kernel_size=3, activation='relu'),
        tf.keras.layers.MaxPooling2D(pool_size=(2, 2)),
        tf.keras.layers.Dropout(0.25),
        # Dense layers.
        tf.keras.layers.Flatten(),
        tf.keras.layers.Dense(256, activation='relu'),
        tf.keras.layers.Dropout(0.5),
        tf.keras.layers.Dense(10, activation='softmax')
    ])
    model.compile(
        optimizer='adam',
        loss='categorical_crossentropy',
        metrics=['accuracy']
    )
    return model

def train_model():
    print("Loading MNIST data...")
    x_train, y_train = load_mnist_data()
    print("Building model...")
    model = build_model()
    
    print("Starting heavy training...")
    start_time = time.perf_counter()
    history = model.fit(x_train, y_train, epochs=5, batch_size=128, verbose=1)
    end_time = time.perf_counter()
    training_time_ms = (end_time - start_time) * 1000
    accuracy = history.history['accuracy'][-1]
    
    print("Training complete.")
    print(f"Training time: {training_time_ms:.2f} ms")
    print(f"Final training accuracy: {(accuracy * 100):.2f}%")

if __name__ == '__main__':
    train_model()
